#include "texture.hpp"
#include "renderer.hpp"
#include "utils.hpp"

#include <spdlog/spdlog.h>

#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <fstream>

using namespace std;
namespace fs = std::filesystem;

shared_ptr<gg::renderer::texture_st> gg::renderer::load_texture_from_raw_data(
    unsigned char *image_data, int width, int height)
{
    auto &device = gg::renderer::impl::device;

    auto upload_pitch = (width * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) &
                        ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
    auto upload_size = height * upload_pitch;

    D3D12_RESOURCE_DESC desc;
    memset(&desc, 0, sizeof(desc));

    D3D12_HEAP_PROPERTIES props;
    memset(&props, 0, sizeof(props));

    // Create texture resource
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource *texture;
    if (!CHECK_RESULT(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                                                      D3D12_RESOURCE_STATE_COPY_DEST, NULL,
                                                      IID_PPV_ARGS(&texture))))
    {
        return nullptr;
    }

    // Create a temporary upload resource to move the data in
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = upload_size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    ID3D12Resource *buffer;
    if (!CHECK_RESULT(device->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
                                                      IID_PPV_ARGS(&buffer))))
    {
        return nullptr;
    }

    // Write pixels into the upload resource
    void *mapped;
    auto range = D3D12_RANGE{0, upload_size};
    if (!CHECK_RESULT(buffer->Map(0, &range, &mapped)))
    {
        return nullptr;
    }

    for (int y = 0; y < height; y++)
        memcpy((void *)((uintptr_t)mapped + y * upload_pitch), image_data + y * width * 4,
               width * 4);
    buffer->Unmap(0, &range);

    // Copy the upload resource content into the real resource
    auto srclocation = D3D12_TEXTURE_COPY_LOCATION{
        .pResource = buffer,
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint = {.Footprint = {.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                          .Width = static_cast<unsigned int>(width),
                                          .Height = static_cast<unsigned int>(height),
                                          .Depth = 1,
                                          .RowPitch = upload_pitch}}};

    auto dstlocation =
        D3D12_TEXTURE_COPY_LOCATION{.pResource = texture,
                                    .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                                    .SubresourceIndex = 0};

    auto barrier = D3D12_RESOURCE_BARRIER{
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {.pResource = texture,
                       .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                       .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
                       .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE}};

    // Create a temporary command queue to do the copy with
    ID3D12Fence *fence;
    if (!CHECK_RESULT(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
        return nullptr;

    auto queuedesc = D3D12_COMMAND_QUEUE_DESC{.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                              .NodeMask = 1};

    ID3D12CommandQueue *command_queue;
    if (!CHECK_RESULT(device->CreateCommandQueue(&queuedesc, IID_PPV_ARGS(&command_queue))))
        return nullptr;

    ID3D12CommandAllocator *command_allocator;
    if (!CHECK_RESULT(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                     IID_PPV_ARGS(&command_allocator))))
        return nullptr;

    ID3D12GraphicsCommandList *command_list;
    if (!CHECK_RESULT(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                command_allocator, nullptr,
                                                IID_PPV_ARGS(&command_list))))
        return nullptr;

    command_list->CopyTextureRegion(&dstlocation, 0, 0, 0, &srclocation, nullptr);
    command_list->ResourceBarrier(1, &barrier);
    if (!CHECK_RESULT(command_list->Close()))
        return nullptr;

    // Execute the copy
    command_queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&command_list);
    if (!CHECK_RESULT(command_queue->Signal(fence, 1)))
        return nullptr;

    // Wait for everything to complete
    auto event = CreateEvent(0, 0, 0, 0);
    fence->SetEventOnCompletion(1, event);
    WaitForSingleObject(event, INFINITE);

    auto result = make_shared<texture_st>(texture, width, height);

    // Create a shader resource view for the texture
    auto srvdesc = D3D12_SHADER_RESOURCE_VIEW_DESC{
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {.MostDetailedMip = 0, .MipLevels = 1}};

    device->CreateShaderResourceView(texture, &srvdesc, result->desc.first);

    // Tear down our temporary command queue and release the upload resource
    command_list->Release();
    command_allocator->Release();
    command_queue->Release();
    CloseHandle(event);
    fence->Release();
    buffer->Release();
    texture->Release();

    return result;
}

shared_ptr<gg::renderer::texture_st> gg::renderer::load_texture_from_memory(
    span<unsigned char> data)
{
    int width, height;
    auto image_data = stbi_load_from_memory(data.data(), data.size(), &width, &height, nullptr, 4);
    if (!image_data)
        return nullptr;

    auto result = load_texture_from_raw_data(image_data, width, height);

    stbi_image_free(image_data);

    return result;
}

shared_ptr<gg::renderer::texture_st> gg::renderer::load_texture_from_file(fs::path filename)
{
    auto stream = basic_ifstream<unsigned char>{filename, ios::binary | ios::ate};
    if (stream.fail())
    {
        SPDLOG_CRITICAL("Failed to load texture {}", filename.string());
        return nullptr;
    }

    auto size = stream.tellg();
    stream.seekg(0, ios::beg);

    auto data = vector<unsigned char>(size);
    if (!stream.read(data.data(), size))
    {
        SPDLOG_CRITICAL("Failed to load texture {}", filename.string());
        return nullptr;
    }

    return load_texture_from_memory(data);
}
