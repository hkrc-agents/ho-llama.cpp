# ho-llama.cpp: HarmonyOS PC CPU port

Official llama.cpp fork with the maintained HarmonyOS affinity/portability fix applied,
SDK examples, native-child entry, HTTP CA-store/TLS candidates and portable CPU build tools.
The upstream API and model mathematics are retained. No fused NPU model integration is included.
[Status and limitations](harmonyos/STATUS.md) | [Validation](harmonyos/VALIDATION.md)

## Standalone CPU tools and SDK

Requires Linux/WSL, Git, Python 3.10+, CMake, Ninja or Make, a host C/C++ compiler for tests,
and the external official HarmonyOS native SDK. No Ollama checkout is needed for this build.

```bash
python3 harmonyos/tools/build_cpu.py \
  --sdk /path/to/sdk/default/openharmony/native \
  --arch x86_64 --out /path/to/new-cpu-build --jobs 4
# Use --arch arm64 for ARM64 cross compilation.
# Optional --native-child builds the standalone official native-child entry too.
python3 -m unittest discover -s harmonyos/native-server/tests -v
```

Outputs: `build/bin/` CPU CLI/server/bench/quantize and bounded tests, `sdk/include/`,
`sdk/lib/`, and `identity.json` with binary hashes. This minimal standalone recipe has
HTTPS and subprocess routing disabled. Build success is not device numerical qualification.
Native-child and managed-service experiments are opt-in; ELF supervision remains the Ollama
product route. Do not infer a new public libllama ABI from the SDK wrapper examples.

## ArkTS/NAPI SDK example

```bash
python3 harmonyos/sdk-example/build.py \
  --source "$PWD" --expected-commit "$(git rev-parse HEAD)" \
  --libraries /path/to/new-cpu-build/build \
  --tools /path/to/command-line-tools/bin --java /path/to/jdk17 \
  --sdk-root /path/to/sdk --arch x86_64 --out /path/to/new-sdk-example
```

The SDK example is self-contained and requires no Ollama HAP, old experiment template,
NPU SDK or weights in Git. Personal signing is external. Legacy qualification commands
in `harmonyos/sdk/README.md` describe historical tests; use this page for independent builds.

## Matched Ollama runner

Ollama adds its own compatibility/model/clef sources to the pinned llama.cpp base. That
composition belongs to [ho-ollama](https://github.com/hkrc-agents/ho-ollama), whose
`harmonyos/tools/build_runtime.py` accepts this checkout explicitly as `--llama-port`
and `--runner-objects`. These sources are not disguised as generic llama.cpp portability.
Keep the tested base pin until a newer source has been independently qualified.

Upstream LICENSE, contributor instructions and history remain intact. Future upstream
submissions require human review; this fork publication does not submit a PR.
