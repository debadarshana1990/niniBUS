#pragma once
#include "niniBUS.h"
#include <cstdint>

// Starts a producer that pushes messages for a single msgID every `interval_seconds` for `run_seconds` seconds.
// Messages will be named "heelo1", "heelo2", ...
void producer_thread(niniBUS& bus, uint32_t msgID, int run_seconds = 20, int interval_seconds = 1);
