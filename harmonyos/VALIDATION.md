# Import validation (2026-11-04)

- 8 host tests of the native-child entry passed, including the permanent removal of the address-space cap.
- Standalone x86_64 OHOS cross build passed: llama-cli, llama-server, llama-bench, llama-quantize, test-rope and test-quantize-fns.
- CPU static SDK libraries and public headers were exported by the new independent build driver.
- Original upstream LICENSE, AGENTS.md, CONTRIBUTING.md and commit ancestry are preserved.
- No private pico source tree, Ollama compatibility hooks, NPU code, SDK files, model weights or signing keys are build inputs to the standalone recipe.

Binary/toolchain hashes are in VALIDATION.json. The target tests were compiled, not executed on a device.
The ArkTS SDK example sources/builder are retained; its HAP was not rebuilt in this import validation.
No new ARM64, numerical, lifecycle or retail/release qualification is claimed.
