#pragma once

#include "renderer.hpp"

#include <d3d12.h>

#include <imgui.h>

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace gg {
namespace renderer {

struct texture {
    gg::renderer::impl::descriptor_pair desc;
    ID3D12Resource *resource;
    int width;
    int height;

    texture(texture &) = delete;

    texture(ID3D12Resource *resource, int width, int height)
        : resource(resource),
          width(width),
          height(height) {
        desc = gg::renderer::impl::alloc_descriptor();
        resource->AddRef();
    };

    ~texture() {
        gg::renderer::impl::free_descriptor(desc);
        resource->Release();
    }

    texture &operator=(const texture &) = delete;
};

/**
 * Simple helper function to load a DX12 texture from 8 bit RGBA pixels already stored in memory
 */
std::shared_ptr<texture> load_texture_from_raw_data(unsigned char *image_data,
                                                    int width,
                                                    int height);

/**
 * Simple helper function to load a DX12 texture from an image file (PNG, JPEG, etc.) loaded into
 * memory
 */
std::shared_ptr<texture> load_texture_from_memory(std::span<unsigned char> data);

/**
 * Simple helper function to load a DX12 texture from an image file on disk
 */
std::shared_ptr<texture> load_texture_from_file(std::filesystem::path filename);

/**
 * Simple helper function to load a DX12 texture from an embedded Windows resource in the current
 * module
 */
std::shared_ptr<texture> load_texture_from_resource(std::string name, std::string type = "DATA");

}
}
