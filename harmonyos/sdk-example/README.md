# Self-contained HarmonyOS CPU SDK example

This downstream HAP embeds libllama in its own process, with no HNP or network
permissions. It does not use Ollama's HAP or a private experiment as a template.
The icon is an original geometric example asset, not the Ollama trademark.

## Build

Copy this directory and its sibling `sdk/` together. Supply an already qualified
x86_64 OHOS static-library build and matching source checkout explicitly:

```bash
python3 build.py --source /path/to/llama-source \
  --expected-commit <full-commit> --libraries /path/to/ohos-static-build \
  --tools /path/to/command-line-tools/bin --java /path/to/jdk17 \
  --out /path/to/new-output
```

The builder serializes runtime/HAP work, records source/archive hashes, rejects
input changes during compilation, retains tool logs, and emits `identity.json`
and `sdk-example.hap`. Outputs must be outside the supplied source/library trees.
`--sdk-root` selects the SDK root containing `default/` when it is not the tools'
sibling `sdk/`. `--resume` requires the exact build ownership marker; arbitrary
old experimental staging directories are rejected. It does not qualify
arbitrary archives or reproduce their upstream build; matching commit alone is
not proof of their provenance. No SDK, compiler, weight, or certificate is bundled.

The default remains x86_64/API23. An explicit `--arch arm64` selects an isolated
arm64-v8a build; supply matching ARM64 archives, never the default x86 libraries.
The builder checks actual HAP native ELF architecture and binds ARM64 retry
ownership separately. Build success is not ARM64 runtime, retail signing,
model accuracy, or long-running lifecycle qualification. 
The native example's ownership and cancellation contract is in [SDK notes](../sdk/README.md).
