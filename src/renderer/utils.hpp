#pragma once

#include <spdlog/spdlog.h>

#include <comdef.h>

#include <string>

#define CHECK_RESULT                                                                               \
    ([](auto hr) [[msvc::forceinline]] {                                                           \
        if (!SUCCEEDED(hr))                                                                        \
        {                                                                                          \
            auto message = std::basic_string<TCHAR>{_com_error{hr}.ErrorMessage()};                \
            SPDLOG_ERROR("DirectX12 call failed with result 0x{:x} - {}", (unsigned long)hr,       \
                         std::string{message.begin(), message.end()});                             \
            return false;                                                                          \
        }                                                                                          \
        return true;                                                                               \
    })
