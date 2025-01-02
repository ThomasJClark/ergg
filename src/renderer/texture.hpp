#pragma once

#include "renderer.hpp"

#include <d3d12.h>

#include <imgui.h>

#include <filesystem>
#include <memory>
#include <span>
#include <utility>

#include <spdlog/spdlog.h>

namespace gg
{
namespace renderer
{

struct texture_st
{
    gg::renderer::impl::descriptor_pair desc;
    ID3D12Resource *resource;
    int width;
    int height;

    texture_st(texture_st &) = delete;

    texture_st(ID3D12Resource *resource, int width, int height)
        : resource(resource), width(width), height(height)
    {
        desc = gg::renderer::impl::alloc_descriptor();
        resource->AddRef();
    };

    ~texture_st()
    {
        gg::renderer::impl::free_descriptor(desc);
        resource->Release();
    }

    texture_st &operator=(const texture_st &) = delete;
};

/**
 * Simple helper function to load a DX12 texture from 8 bit RGBA pixels already stored in memory
 */
std::shared_ptr<texture_st> load_texture_from_raw_data(unsigned char *image_data, int width,
                                                       int height);

/**
 * Simple helper function to load a DX12 texture from an image file (PNG, JPEG, etc.) loaded into
 * memory
 */
std::shared_ptr<texture_st> load_texture_from_memory(std::span<unsigned char> data);

/**
 * Simple helper function to load a DX12 texture from an image file on disk
 */
std::shared_ptr<texture_st> load_texture_from_file(std::filesystem::path filename);

}
}
