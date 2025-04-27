#pragma once

#include <functional>
#include <string>

namespace gg {
namespace logs {

size_t size();

/**
 * Append a new log message to the ring buffer, overwriting the oldest message if the buffer is
 * full
 */
std::string &append();

void log(std::string &&message);

void for_each(std::function<void(const std::string &)> callback);

}
}
