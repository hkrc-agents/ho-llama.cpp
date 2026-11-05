#include <AbilityKit/native_child_process.h>
#include <nlohmann/json.hpp>
#include <cstdio>

extern "C" void Main(NativeChildProcess_Args);
int llama_server(int argc, char **argv) {
    nlohmann::json values = nlohmann::json::array();
    for (int i = 0; i < argc; ++i) values.push_back(argv[i]);
    printf("STUB_ARGV %s\n", values.dump().c_str());
    return 7;
}
int main(int argc, char **argv) {
    Main({argc > 1 ? argv[1] : nullptr, {nullptr}});
}
