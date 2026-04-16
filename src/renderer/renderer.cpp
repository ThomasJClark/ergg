#include "renderer.hpp"

#include <elden-x/graphics.hpp>
#include <elden-x/window.hpp>

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>
#include <imgui.h>

#include <spdlog/spdlog.h>

#include <string>
#include <thread>
#include <vector>

using namespace std;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, uint32_t, WPARAM, LPARAM);

ID3D12Device *gg::renderer::impl::device;

static constexpr uint32_t srv_descriptor_count = 1024;

static D3D12_CPU_DESCRIPTOR_HANDLE heap_start_cpu;
static D3D12_GPU_DESCRIPTOR_HANDLE heap_start_gpu;
static uint32_t heap_increment_size;
static vector<int32_t> free_indexes;

struct frame_context {
    ID3D12CommandAllocator *command_allocator{nullptr};
    ID3D12Resource *render_target{nullptr};
    D3D12_CPU_DESCRIPTOR_HANDLE render_target_descriptor{};
    uint64_t fence_value{0};
};

static vector<frame_context> frames;
static ID3D12GraphicsCommandList *command_list{nullptr};
static ID3D12DescriptorHeap *rtv_heap{nullptr};
static ID3D12DescriptorHeap *srv_heap{nullptr};
static ID3D12Fence *fence{nullptr};
static HANDLE fence_event{nullptr};
static uint64_t fence_value{0};

static bool initialized{false};

static function<void()> g_initialize_callback;
static function<void()> g_render_callback;

static WNDPROC original_wndproc;
static LRESULT wndproc_hook(HWND hwnd, uint32_t msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
    return CallWindowProc(original_wndproc, hwnd, msg, wparam, lparam);
}

static HRESULT (*original_present)(IDXGISwapChain3 *, uint32_t, uint32_t);
static HRESULT (*original_resize_buffers)(IDXGISwapChain *, uint32_t, uint32_t, uint32_t, DXGI_FORMAT, uint32_t);

static void wait_for_gpu(ID3D12CommandQueue *command_queue) {
    fence_value++;
    command_queue->Signal(fence, fence_value);
    fence->SetEventOnCompletion(fence_value, fence_event);
    WaitForSingleObject(fence_event, INFINITE);
}

static void setup_render_targets(IDXGISwapChain3 *swap_chain) {
    auto rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
    auto rtv_size = gg::renderer::impl::device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (size_t i = 0; i < frames.size(); i++) {
        swap_chain->GetBuffer(i, IID_PPV_ARGS(&frames[i].render_target));
        gg::renderer::impl::device->CreateRenderTargetView(frames[i].render_target, nullptr, rtv_handle);
        frames[i].render_target_descriptor = rtv_handle;
        rtv_handle.ptr += rtv_size;
    }
}

static void release_render_targets() {
    for (auto &frame : frames) {
        if (frame.render_target) {
            frame.render_target->Release();
            frame.render_target = nullptr;
        }
    }
}

static bool initialize_renderer(IDXGISwapChain3 *swap_chain, ID3D12CommandQueue *command_queue) {
    swap_chain->GetDevice(IID_PPV_ARGS(&gg::renderer::impl::device));
    auto device = gg::renderer::impl::device;

    DXGI_SWAP_CHAIN_DESC sc_desc;
    swap_chain->GetDesc(&sc_desc);
    uint32_t buffer_count = sc_desc.BufferCount;

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .NumDescriptors = buffer_count,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 1,
        };
        if (device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtv_heap)) != S_OK) {
            SPDLOG_ERROR("renderer: failed to create RTV heap");
            return false;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = srv_descriptor_count,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        };
        if (device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&srv_heap)) != S_OK) {
            SPDLOG_ERROR("renderer: failed to create SRV heap");
            return false;
        }
        heap_start_cpu = srv_heap->GetCPUDescriptorHandleForHeapStart();
        heap_start_gpu = srv_heap->GetGPUDescriptorHandleForHeapStart();
        heap_increment_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        free_indexes.reserve(srv_descriptor_count);
        for (int32_t n = srv_descriptor_count; n > 0; n--) {
            free_indexes.push_back(n);
        }
    }

    frames.resize(buffer_count);
    for (auto &frame : frames) {
        if (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                           IID_PPV_ARGS(&frame.command_allocator)) != S_OK) {
            SPDLOG_ERROR("renderer: failed to create command allocator");
            return false;
        }
    }

    if (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  frames[0].command_allocator, nullptr,
                                  IID_PPV_ARGS(&command_list)) != S_OK ||
        command_list->Close() != S_OK) {
        SPDLOG_ERROR("renderer: failed to create command list");
        return false;
    }

    if (device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)) != S_OK) {
        SPDLOG_ERROR("renderer: failed to create fence");
        return false;
    }
    fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    setup_render_targets(swap_chain);

    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    g_initialize_callback();

    ImGui_ImplDX12_InitInfo init_info;
    init_info.Device = device;
    init_info.NumFramesInFlight = buffer_count;
    init_info.RTVFormat = sc_desc.BufferDesc.Format;
    init_info.SrvDescriptorHeap = srv_heap;
    init_info.SrvDescriptorAllocFn = [](auto, auto *cpu, auto *gpu) {
        tie(*cpu, *gpu) = gg::renderer::impl::alloc_descriptor();
    };
    init_info.SrvDescriptorFreeFn = [](auto, auto cpu, auto gpu) {
        gg::renderer::impl::free_descriptor({cpu, gpu});
    };

    auto hwnd = er::CS::CSWindow::instance()->hwnd;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(&init_info);
    ImGui_ImplDX12_CreateDeviceObjects();
    ImGui::GetMainViewport()->PlatformHandleRaw = hwnd;

    original_wndproc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)wndproc_hook);

    SPDLOG_INFO("renderer: initialized");
    return true;
}

static HRESULT present_hook(IDXGISwapChain3 *swap_chain, uint32_t sync_interval, uint32_t flags) {
    auto gxglobals = er::GXBS::globals::instance();
    if (!gxglobals) return original_present(swap_chain, sync_interval, flags);

    auto command_queue = gxglobals->get_command_queue();
    if (!command_queue) return original_present(swap_chain, sync_interval, flags);

    if (!initialized) {
        initialized = initialize_renderer(swap_chain, command_queue);
        if (!initialized) return original_present(swap_chain, sync_interval, flags);
    }

    uint32_t frame_index = swap_chain->GetCurrentBackBufferIndex();
    auto &frame = frames[frame_index];

    if (frame.fence_value > 0) {
        fence->SetEventOnCompletion(frame.fence_value, fence_event);
        WaitForSingleObject(fence_event, INFINITE);
    }

    frame.command_allocator->Reset();
    command_list->Reset(frame.command_allocator, nullptr);

    auto barrier = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {.pResource = frame.render_target,
                       .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                       .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                       .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET}};
    command_list->ResourceBarrier(1, &barrier);
    command_list->OMSetRenderTargets(1, &frame.render_target_descriptor, FALSE, nullptr);
    command_list->SetDescriptorHeaps(1, &srv_heap);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    g_render_callback();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    command_list->ResourceBarrier(1, &barrier);
    command_list->Close();

    command_queue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList *const *>(&command_list));

    fence_value++;
    frame.fence_value = fence_value;
    command_queue->Signal(fence, fence_value);

    return original_present(swap_chain, sync_interval, flags);
}

static HRESULT resize_buffers_hook(IDXGISwapChain *swap_chain,
                                   uint32_t buffer_count,
                                   uint32_t width,
                                   uint32_t height,
                                   DXGI_FORMAT new_format,
                                   uint32_t flags) {
    if (initialized) {
        auto gxglobals = er::GXBS::globals::instance();
        if (gxglobals) {
            auto cq = gxglobals->get_command_queue();
            if (cq) wait_for_gpu(cq);
        }
        release_render_targets();
    }

    auto hr = original_resize_buffers(swap_chain, buffer_count, width, height, new_format, flags);

    if (initialized) {
        setup_render_targets((IDXGISwapChain3 *)swap_chain);
    }

    return hr;
}

gg::renderer::impl::descriptor_pair gg::renderer::impl::alloc_descriptor() {
    auto index = free_indexes.back();
    free_indexes.pop_back();
    return {D3D12_CPU_DESCRIPTOR_HANDLE{heap_start_cpu.ptr + (index * heap_increment_size)},
            D3D12_GPU_DESCRIPTOR_HANDLE{heap_start_gpu.ptr + (index * heap_increment_size)}};
}

void gg::renderer::impl::free_descriptor(gg::renderer::impl::descriptor_pair pair) {
    int32_t index = (int32_t)((pair.first.ptr - heap_start_cpu.ptr) / heap_increment_size);
    free_indexes.push_back(index);
}

void gg::renderer::initialize(function<void()> initialize_callback,
                              function<void()> render_callback) {
    g_initialize_callback = initialize_callback;
    g_render_callback = render_callback;

    er::GXBS::globals *gxglobals;
    while (!(gxglobals = er::GXBS::globals::instance())) {
        YieldProcessor();
    }

    IDXGISwapChain3 *swap_chain;
    while (!(swap_chain = gxglobals->get_swap_chain())) {
        YieldProcessor();
    }

    struct swap_chain_vtable {
        void *QueryInterface;
        void *AddRef;
        void *Release;
        void *SetPrivateData;
        void *SetPrivateDataInterface;
        void *GetPrivateData;
        void *GetParent;
        void *GetDevice;
        void *Present;
        void *GetBuffer;
        void *SetFullscreenState;
        void *GetFullscreenState;
        void *GetDesc;
        void *ResizeBuffers;
    };

    auto vtable = *reinterpret_cast<swap_chain_vtable **>(swap_chain);

    DWORD old_protect;
    VirtualProtect(&vtable->Present, sizeof(void *), PAGE_EXECUTE_READWRITE, &old_protect);
    original_present = (decltype(original_present))vtable->Present;
    vtable->Present = (void *)present_hook;
    VirtualProtect(&vtable->Present, sizeof(void *), old_protect, &old_protect);

    VirtualProtect(&vtable->ResizeBuffers, sizeof(void *), PAGE_EXECUTE_READWRITE, &old_protect);
    original_resize_buffers = (decltype(original_resize_buffers))vtable->ResizeBuffers;
    vtable->ResizeBuffers = (void *)resize_buffers_hook;
    VirtualProtect(&vtable->ResizeBuffers, sizeof(void *), old_protect, &old_protect);

    SPDLOG_INFO("renderer: hooks installed");
}