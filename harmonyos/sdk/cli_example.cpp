#include "session_example.h"
#include <atomic>
#include <cstdio>
#include <sys/stat.h>
int main(int argc, char ** argv) {
    if (argc != 3) return 2;
    if (mkdir(argv[2], 0700) != 0) return 3;
    std::atomic<bool> cancelled{false};
    try { const auto report = pico_sdk_suite(argv[1], argv[2], cancelled); std::puts(report.c_str());
          return report.find("\"pass\": true") != std::string::npos ? 0 : 1; }
    catch (...) { return 4; }
}
