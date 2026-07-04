#pragma once
#include "niniBUS.h"
#include <cstdint>

// Starts a consumer that repeatedly pulls messages with given msgID every `interval_seconds` for `run_seconds`.
// This is non-blocking: if pull_msg returns false the consumer will wait until next cycle.
void consumer_thread(niniBUS& bus, uint32_t msgID, int consumer_id, int run_seconds = 60, int interval_seconds = 2);

