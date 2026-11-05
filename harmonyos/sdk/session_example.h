#pragma once
#include <atomic>
#include <string>
// Example/test harness, not a replacement ABI for libllama. Applications link
// the pinned upstream llama.h and libraries directly.
std::string pico_sdk_suite(const std::string & model, const std::string & directory,
                           std::atomic<bool> & cancelled);
