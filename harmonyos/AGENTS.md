# HarmonyOS port maintenance

Keep only source, documentation and tests needed to build and run the HarmonyOS port and suitable for upstream review.
Preserve upstream behavior on other platforms, licenses and commit history.
Keep whole-project architecture, model/operator optimization, private experiments and historical session records outside this fork.
Dependencies are external: document official download/install links and required versions; do not commit SDKs, DDKs, installers or downloaded packages.
Keep generated binaries, HAP/HNP/OMC, weights, converted tensors, logs, traces and test-result receipts outside tracked source.
Put new build and test outputs in fresh subdirectories of the repository's Git-ignored `build/` or `target/`.
Use explicit toolchain, source, target and output arguments; no personal device or workspace paths.
Do not set RLIMIT_AS. Keep bounded execution and owned-process cleanup in device tests.
Serialize tests that share a device. Record the actual architecture, OS/SDK, caller identity and staged binary hash. Verify the expected remote output and clean up only the files and processes owned by the run.
Treat compilation, host tests, target numerical checks and release qualification as separate results. Report failures, skips and unexecuted cases separately.
For CPU comparisons, record the model hash, build, thread count and timing scope. Use the same inputs and benchmark boundary when comparing candidates. Keep Ollama-specific runner patches in ho-ollama.
Do not submit upstream PRs automatically. Publishing this downstream fork does not imply upstream acceptance.
