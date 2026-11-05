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

`harmonyos/sdk-example/` supplies an independent ArkTS/NAPI example. Its system
picker imports a model into that application's private directory; its NAPI
entry dispatches asynchronous native work and rejects a second active job.
It requests no network permission and contains no HNP package.

## Build and test

Use ../../HARMONYOS.md and ../sdk-example/README.md with explicit source, library and SDK inputs. The example handles initialization, context ownership, asynchronous work and cooperative cancellation. Select a local GGUF model through the example app; no weights or generated traces are committed.
Host tests and target compilation do not qualify every model or device. Keep any model-specific numerical reference and experimental mathematical controls outside the platform port.
