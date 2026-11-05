# CPU follow-up work

- Complete independent ARM64 numerical and lifecycle qualification before release.
- Review ARM BF16 instruction availability and the pinned ggml CPU dispatch/build flags after the FP16 model optimization work. Keep the current CPU baseline unchanged during repository migration.
- Rebase generic portability changes separately from platform-owned examples and service tooling.
- Revalidate a newer upstream base before changing the lock used by the matched Ollama runtime.

Fused NPU model integration is outside the current repository preparation scope.
