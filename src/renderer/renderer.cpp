#include "renderer.hpp"

#include <elden-x/menu/menu_man.hpp>
#include <elden-x/task.hpp>
#include <elden-x/window.hpp>

#include <kiero.h>

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

using namespace std;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, unsigned int, WPARAM, LPARAM);

ID3D12Device *gg::renderer::impl::device;

static constexpr unsigned int srv_descriptor_count = 1024;

static D3D12_CPU_DESCRIPTOR_HANDLE heap_start_cpu;
static D3D12_GPU_DESCRIPTOR_HANDLE heap_start_gpu;
static unsigned int increment_size;
static vector<int> free_indexes;

static ID3D12CommandQueue *command_queue{nullptr};
static IDXGISwapChain3 *swap_chain{nullptr};

struct render_task : public er::CS::CSEzTask {
        private:
    /**
     * True if the render has been set up and we can make draw calls each frame
     */
    bool initialized{false};

    /**
     * True if the window was resized on the previous frame. In the FrameBegin step, the render
     * targets will be re-allocated.
     */
    bool resized{false};
    pair<int, int> prev_window_size;

    /**
     * Callback to overlay rendering methods passed from dllmain
     */
    function<void()> initialize_callback;
    function<void()> render_callback;

    struct render_target {
        ID3D12Resource *resource;
        D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle;
    };

    vector<render_target> render_targets;

    ID3D12DescriptorHeap *render_descriptor_heap{nullptr};
    ID3D12DescriptorHeap *back_buffers_descriptor_heap{nullptr};

    ID3D12CommandAllocator *command_allocator{nullptr};
    ID3D12GraphicsCommandList *command_list{nullptr};
    ID3D12DescriptorHeap *srv_descriptor_heap{nullptr};

    void release_render_targets() {
        for (auto &frame : render_targets) {
            frame.resource->Release();
        }
    }

    void setup_render_targets() {
        for (size_t i = 0; i < render_targets.size(); i++) {
            swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i].resource));
            gg::renderer::impl::device->CreateRenderTargetView(render_targets[i].resource, nullptr,
                                                               render_targets[i].descriptor_handle);
        }
    }

    void initialize() {
        swap_chain->GetDevice(IID_PPV_ARGS(&gg::renderer::impl::device));

        // Allocate a descriptor heap for the frames
        {
            DXGI_SWAP_CHAIN_DESC swap_chain_desc;
            swap_chain->GetDesc(&swap_chain_desc);
            render_targets.resize(swap_chain_desc.BufferCount);

            auto desc = D3D12_DESCRIPTOR_HEAP_DESC{
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .NumDescriptors = static_cast<unsigned int>(render_targets.size()),
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            };
            if (gg::renderer::impl::device->CreateDescriptorHeap(
                    &desc, IID_PPV_ARGS(&render_descriptor_heap)) != S_OK) {
                return;
            }
        }

        // Allocate a descriptor heap for shader resource values (textures and other buffers)
        {
            auto desc = D3D12_DESCRIPTOR_HEAP_DESC{
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .NumDescriptors = srv_descriptor_count,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            };
            if (gg::renderer::impl::device->CreateDescriptorHeap(
                    &desc, IID_PPV_ARGS(&srv_descriptor_heap)) != S_OK) {
                return;
            }
        }
        {
            auto desc = srv_descriptor_heap->GetDesc();
            heap_start_cpu = srv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
            heap_start_gpu = srv_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
            increment_size =
                gg::renderer::impl::device->GetDescriptorHandleIncrementSize(desc.Type);
            free_indexes.reserve((int)desc.NumDescriptors);
            for (int n = desc.NumDescriptors; n > 0; n--) {
                free_indexes.push_back(n);
            }
        }

        // Create the other bullshit you need to use DX12
        if (gg::renderer::impl::device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator)) != S_OK) {
            return;
        }

        if (gg::renderer::impl::device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                          command_allocator, NULL,
                                                          IID_PPV_ARGS(&command_list)) != S_OK ||
            command_list->Close() != S_OK) {
            return;
        }

        auto back_buffers_heap_desc = D3D12_DESCRIPTOR_HEAP_DESC{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = static_cast<unsigned int>(render_targets.size()),
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 1,
        };
        if (gg::renderer::impl::device->CreateDescriptorHeap(
                &back_buffers_heap_desc, IID_PPV_ARGS(&back_buffers_descriptor_heap)) != S_OK) {
            return;
        }

        auto descriptor_handle = back_buffers_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
        const auto increment_size = gg::renderer::impl::device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (size_t i = 0; i < render_targets.size(); i++) {
            render_targets[i].descriptor_handle.ptr = descriptor_handle.ptr + i * increment_size;
        }

        setup_render_targets();

        ImGui::CreateContext();
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        initialize_callback();

        ImGui_ImplDX12_InitInfo init_info;
        init_info.Device = gg::renderer::impl::device;
        init_info.NumFramesInFlight = render_targets.size();
        init_info.RTVFormat = DXGI_FORMAT_R10G10B10A2_UNORM;
        init_info.SrvDescriptorHeap = render_descriptor_heap;
        init_info.SrvDescriptorAllocFn = [](auto, auto *cpu_handle, auto *gpu_handle) {
            tie(*cpu_handle, *gpu_handle) = gg::renderer::impl::alloc_descriptor();
        };
        init_info.SrvDescriptorFreeFn = [](auto, auto cpu_handle, auto gpu_handle) {
            gg::renderer::impl::free_descriptor({cpu_handle, gpu_handle});
        };

        auto cs_window = er::CS::CSWindow::instance();

        ImGui_ImplWin32_Init(cs_window->hwnd);
        ImGui_ImplDX12_Init(&init_info);
        ImGui_ImplDX12_CreateDeviceObjects();

        ImGui::GetMainViewport()->PlatformHandleRaw = cs_window->hwnd;

        initialized = true;
    }

    void frame_begin() {
        if (resized) {
            resized = false;
            setup_render_targets();
        }
    }

    void draw_step() {
        if (!command_queue || !swap_chain) {
            return;
        }

        // Set up ImGui the first time this is called
        if (!initialized) {
            initialize();
        }

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();
        render_callback();
        ImGui::EndFrame();
        ImGui::Render();

        auto &render_target = render_targets[swap_chain->GetCurrentBackBufferIndex()];

        command_allocator->Reset();

        auto resource_barrier = D3D12_RESOURCE_BARRIER{
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {.pResource = render_target.resource,
                           .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                           .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                           .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET}};

        command_list->Reset(command_allocator, nullptr);
        command_list->ResourceBarrier(1, &resource_barrier);
        command_list->OMSetRenderTargets(1, &render_target.descriptor_handle, FALSE, nullptr);
        command_list->SetDescriptorHeaps(1, &render_descriptor_heap);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list);

        resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        command_list->ResourceBarrier(1, &resource_barrier);
        command_list->Close();

        command_queue->ExecuteCommandLists(
            1, reinterpret_cast<ID3D12CommandList *const *>(&command_list));
    }

    void frame_end() {
        auto window = er::CS::CSWindow::instance();
        if (window && initialized && window->window_size != prev_window_size) {
            resized = true;
            prev_window_size = window->window_size;
            release_render_targets();
        }
    }

        public:
    render_task(function<void()> initialize_callback, function<void()> render_callback)
        : initialize_callback(initialize_callback),
          render_callback(render_callback) {}

    virtual void execute(er::FD4::task_data *data) override {
        switch (data->group) {
            case er::task_group::FrameBegin:
                frame_begin();
                break;

            case er::task_group::DrawStep:
                draw_step();
                break;

            case er::task_group::FrameEnd:
                frame_end();
                break;
        }
    }
};

/**
 * WNDPROC callback
 *
 * Handles ImGui events, then defers the the game's default WNDPROC
 *
 * https://learn.microsoft.com/en-us/windows/win32/api/winuser/nc-winuser-wndproc
 */
static WNDPROC wndproc;
static LRESULT wndproc_hook(HWND hwnd, unsigned int msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    return CallWindowProc(wndproc, hwnd, msg, wparam, lparam);
}

/**
 * Hook for IDXGISwapChain::Present()
 *
 * Stores a pointer to the swap chain so we can render a custom overlay
 *
 * https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present
 */
static HRESULT(APIENTRY *swap_chain_present)(IDXGISwapChain3 *, unsigned int, unsigned int);
static HRESULT APIENTRY swap_chain_present_hook(IDXGISwapChain3 *_this,
                                                unsigned int sync_interval,
                                                unsigned int flags) {
    if (command_queue) {
        swap_chain = _this;
    }

    return swap_chain_present(_this, sync_interval, flags);
}

/**
 * Hook for ID3D12CommandQueue::ExecuteCommandLists()
 *
 * Stores a pointer to the command queue for use in render callbacks, then calls the original
 * implementation.
 *
 * https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists
 */
static void(APIENTRY *command_queue_execute_command_lists)(ID3D12CommandQueue *,
                                                           unsigned int,
                                                           ID3D12CommandList *);
static void command_queue_execute_command_lists_hook(ID3D12CommandQueue *_this,
                                                     UINT command_list_count,
                                                     ID3D12CommandList *command_lists) {
    // Don't hijack the command queue until we have evidence it's ready. The easiest way to do that
    // is by checking that something that requires it is initialized.
    if (!command_queue && er::CS::CSMenuManImp::instance() != nullptr) {
        command_queue = _this;
    }

    command_queue_execute_command_lists(_this, command_list_count, command_lists);
}

gg::renderer::impl::descriptor_pair gg::renderer::impl::alloc_descriptor() {
    auto index = free_indexes.back();
    free_indexes.pop_back();
    return gg::renderer::impl::descriptor_pair{heap_start_cpu.ptr + (index * increment_size),
                                               heap_start_gpu.ptr + (index * increment_size)};
}

void gg::renderer::impl::free_descriptor(gg::renderer::impl::descriptor_pair pair) {
    int index = (int)((pair.first.ptr - heap_start_cpu.ptr) / increment_size);
    free_indexes.push_back(index);
}

void gg::renderer::initialize(function<void()> initialize_callback,
                              function<void()> render_callback) {
    auto hwnd = er::CS::CSWindow::instance()->hwnd;
    wndproc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc_hook);

    // https://github.com/Rebzzel/kiero/blob/1.2.12/METHODSTABLE.txt
    kiero::init(kiero::RenderType::D3D12);
    kiero::bind(54, (void **)&command_queue_execute_command_lists,
                command_queue_execute_command_lists_hook);
    kiero::bind(140, (void **)&swap_chain_present, swap_chain_present_hook);

    // Register a task to draw the overlay. This could also be done in the DX12 hook, but that
    // tends not to play nicely with capture software and other overlays.
    static auto task = render_task{initialize_callback, render_callback};
    auto task_man = er::CS::CSTask::instance();
    task_man->register_task(er::task_group::FrameBegin, task);
    task_man->register_task(er::task_group::DrawStep, task);
    task_man->register_task(er::task_group::FrameEnd, task);
}
