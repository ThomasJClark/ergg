#include "logs.hpp"

extern std::array<gg::logs::log_entry_st, 2048> gg::logs::logs_ring;
extern size_t gg::logs::logs_begin = 0;
extern size_t gg::logs::logs_size = 0;
