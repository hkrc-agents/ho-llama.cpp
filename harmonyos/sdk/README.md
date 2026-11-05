# HarmonyOS CPU libllama example

This is an application-integration example and qualification harness, not a
replacement for the upstream `llama.h` ABI. It links the same pinned runner
source and CPU static archives as the HarmonyOS deployment. No server, HTTP,
HNP launcher or OMC engine is involved.

## Ownership and threading

`session_example.cpp` demonstrates these rules:

- Initialize the backend before loading models; free it after all models and
  contexts have been released.
- Keep the model alive for every context that uses it. Independent contexts
  may share that model, but each context has one execution owner. Do not call
  decode concurrently on the same context.
- Run tokenization, prefill, decode, sampling and token-to-piece conversion on
  a native worker, not the ArkUI thread.
- Use the backend abort callback for cooperative cancellation. Discard the
  interrupted context rather than assuming its state was rolled back. A new
  context can reuse the still-live model.
- Let the calling application manage its own background lifecycle. This SDK
  does not borrow the Ollama application's identity or background task.

`apps/sdk-example/` supplies an independent ArkTS/NAPI example. Its system
picker imports a model into that application's private directory; its NAPI
entry dispatches asynchronous native work and rejects a second active job.
It requests no network permission and contains no HNP package.

## Reproduce the bounded qualification fixture

From the project root:

```bash
python3 scripts/build_harmonyos_sdk_example.py --out artifacts/<new-sdk-build>
python3 scripts/stage_harmonyos_e5_model.py --kind sdk \
  --hap artifacts/<new-sdk-build>/sdk-example.hap --out artifacts/<new-model-stage>
python3 scripts/verify_harmonyos_sdk_suite.py --out artifacts/<new-sdk-run>
python3 scripts/verify_harmonyos_sdk_suite.py --cancel --out artifacts/<new-cancel-run>
```

The staging script intentionally selects the known E4 export, not an arbitrary
user file. It is an internal emulator test helper, not a general import CLI.
The suite runner temporarily stops the normal Ollama services and restores them
afterward. Keep the normal Ollama application available before running it.

The fixture uses Qwen3.5-0.8B Q4_K_M, context 512, one/two CPU threads, four
teacher-forced full-vocabulary logits vectors per thread configuration, bounded
greedy generation, two parallel contexts, abort/recovery and model reload.
These are test bounds, not public SDK or model-support limits. The example's
1 GiB picker bound likewise does not limit libllama.

Numerical acceptance uses the project's Linux FP64-RoPE baseline and exact
FP32 logits bytes, with cosine, maximum absolute error and both KL directions
also reported by `scripts/compare_harmonyos_sdk_logits.py`. Original-trig Linux
results remain a separate control. Do not extrapolate one short-prefix fixture
to all models or long-context numerical qualification.

ARM64, HiShell and final-device lifecycle qualification belong to E6 in
`design/HARMONYOS_CPU_EXECUTION_PLAN.md`.
