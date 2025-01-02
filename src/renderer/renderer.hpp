#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>

#include <functional>

namespace gg
{
namespace renderer
{
namespace impl
{

extern ID3D12Device *device;

typedef std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> descriptor_pair;

descriptor_pair alloc_descriptor();
void free_descriptor(descriptor_pair);

}

/**
 * Hooks the rendering of the game and sets up custom UI callback that can render stuff using Dear
 * ImGui
 */
void initialize(std::function<void()> initialize_callback, std::function<void()> render_callback);

}
}
