#include <llama.h>
#include <cstdio>

int main() {
    llama_backend_init();
    const auto model = llama_model_default_params();
    const auto context = llama_context_default_params();
    std::printf("llama SDK initialized; model_gpu_layers=%d context=%u\n", model.n_gpu_layers, context.n_ctx);
    llama_backend_free();
    return 0;
}
