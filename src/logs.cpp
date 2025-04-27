#include "logs.hpp"

#include <array>
#include <memory>

using namespace std;

static auto logs_ring_ptr = make_unique<array<string, 50>>();
static size_t logs_begin = 0;
static size_t logs_size = 0;

size_t gg::logs::size() { return logs_size; }

string &gg::logs::append() {
    auto &logs_ring = *logs_ring_ptr;

    if (logs_size == logs_ring.size()) {
        auto &oldest_entry = logs_ring[logs_begin];
        logs_begin = (logs_begin + 1) % logs_ring.size();
        return oldest_entry;
    }
    return logs_ring[logs_size++];
}

void gg::logs::log(string &&message) { append() = move(message); }

void gg::logs::for_each(function<void(const string &)> callback) {
    auto &logs_ring = *logs_ring_ptr;

    auto limit = logs_begin + logs_size;
    for (size_t i = logs_begin; i < limit; i++) {
        callback(logs_ring[i % logs_ring.size()]);
    }
}