#include "renderer.hpp"

#include <elden-x/graphics.hpp>
#include <elden-x/task.hpp>
#include <elden-x/window.hpp>

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

#include <kiero.h>
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

struct render_task : public er::CS::CSEzTask {
private:
    /**
     * True if the render has been set up and we can make draw calls each frame
     */
    bool initialized{false};

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

public:
    void release_render_targets() {
        for (auto &frame : render_targets) {
            frame.resource->Release();
        }
    }

    void setup_render_targets() {
        auto swap_chain = er::GXBS::globals::instance()->get_swap_chain();
        for (size_t i = 0; i < render_targets.size(); i++) {
            swap_chain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i].resource));
            gg::renderer::impl::device->CreateRenderTargetView(render_targets[i].resource, nullptr,
                                                               render_targets[i].descriptor_handle);
        }
    }

    void initialize() {
        auto swap_chain = er::GXBS::globals::instance()->get_swap_chain();
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

public:
    render_task() {}

    render_task(function<void()> initialize_callback, function<void()> render_callback)
        : initialize_callback(initialize_callback),
          render_callback(render_callback) {}

    virtual void execute(er::FD4::task_data *data,
                         er::FD4::task_group group,
                         er::FD4::task_affinity affinity) override {
        auto gxglobals = er::GXBS::globals::instance();
        auto command_queue = gxglobals->get_command_queue();
        auto swap_chain = gxglobals->get_swap_chain();
        if (!command_queue || !swap_chain) return;

        // Set up ImGui the first time this is called
        if (!initialized) initialize();
        if (!initialized) return;

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

static render_task task;

/**
 * Hook for IDXGISwapChain::ResizeBuffers()
 *
 * Resizes our overlay render targets when the game window is resized
 *
 * https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-resizebuffers
 */
static HRESULT(APIENTRY *swap_chain_resize_buffers)(
    IDXGISwapChain3 *, unsigned int, unsigned int, unsigned int, DXGI_FORMAT, unsigned int);
static HRESULT swap_chain_resize_buffers_hook(IDXGISwapChain3 *_this,
                                              unsigned int buffer_count,
                                              unsigned int width,
                                              unsigned int height,
                                              DXGI_FORMAT new_format,
                                              unsigned int flags) {
    task.release_render_targets();
    auto hr = swap_chain_resize_buffers(_this, buffer_count, width, height, new_format, flags);
    task.setup_render_targets();
    return hr;
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

    task = render_task{initialize_callback, render_callback};
    er::CS::CSTask::instance()->register_task(er::FD4::task_group::DrawBegin, task);

    kiero::init(kiero::RenderType::D3D12);
    kiero::bind(145, (void **)&swap_chain_resize_buffers, swap_chain_resize_buffers_hook);
    kiero::shutdown();
}
