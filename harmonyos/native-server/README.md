# OHOS native llama-server child

`entry.cpp` exports `extern "C" void Main(NativeChildProcess_Args)` for
`OH_Ability_StartNativeChildProcess("libllama-server-child.so:Main", ...)`.
Load it only in a dedicated official native child, never in the UI process.
It calls the pinned `llama_server(argc, argv)` directly through
`llama-server-impl`; it does not exec, fork, or reimplement HTTP/inference.
`native_server.cmake` is generic and has no Ollama source dependency.
The integration wrapper composes it with the existing, unchanged Ollama hook.

`entryParams` is a NUL-terminated UTF-8 JSON object, at most 65536 bytes:

```json
{"schema":1,"argv":["--host","127.0.0.1","--port","18189"],"log_path":"/data/storage/el2/base/files/server-run1.log","receipt_path":"/data/storage/el2/base/files/server-run1.jsonl"}
```

Exactly these four keys are accepted. `argv` contains at most 256 string arguments,
excluding argv[0] (the wrapper inserts `llama-server`). Embedded NUL is rejected.
Spaces, quotes, newlines and Unicode inside arguments are passed literally, with
no shell parsing. For a model, add `"-m","/absolute/model.gguf"` and its normal
upstream options. Without a model this pinned server enters router mode, whose
route initialization rejects this subprocess-disabled build before it starts
listening. Thus the example specifies the wire format, not a working no-model
health probe. Use `"argv":["--version"]` for an SO entry/load probe (upstream may
exit directly); use a small explicit model for `/health` and parent-death tests.
The wrapper adds no flags and does not enable router subprocess spawning.
The unchanged upstream `ggml_print_backtrace` still imports `fork`/`execlp` for
its debugger fallback on a crash. Thus the direct, non-exec startup does not prove
that every abnormal path in the complete linked library is spawn-free.

Both output paths must be absolute, different, writable by this child, and not
already exist. Parent directories must already exist and belong to the caller;
use a fresh private run directory per start. Files use exclusive/no-follow open
and mode 0600. stdout/stderr go to the log; stdin is `/dev/null`. The FD list is
unused (no Binder FD transport is required). No cwd or environment is modified.

The wrapper does not set or require RLIMIT_AS. As of 2026-11-04 the user
permanently removed virtual-address caps and per-run exception requests.
Argument validation, file ownership, lifecycle receipts and cleanup remain.
NDJSON receipts contain schema, phase (`enter`, `return`, `error`),
PID/PPID/UID/GID, rc, and `address_space_policy: "not-set"`.
`enter` means about to call the server,
not HTTP readiness. `return` records its return value, then the child uses
`_exit(rc)` after flushing. Fatal signals, parent death, or upstream `exit()` may
leave only `enter`; the parent must use the official exit callback and health
check, not infer a clean stop from a receipt. Early errors appear on inherited
stderr if the private log cannot yet be opened. This is not a stop/ACK protocol.

From the integration workspace:

```bash
python3 scripts/build_llama_native_child.py --jobs 4 \
  --out artifacts/llama-native-child-<fresh-run>
python3 scripts/verify_llama_native_child.py \
  --out artifacts/llama-native-child-<fresh-run>
```

The builder takes only the runtime build lock. It uses a fresh ext4 build tree,
existing patched runner HEAD `631109b34da437a3c4a5ebd75091d677671392e3`, OHOS SDK,
PIC static dependencies, static C++ runtime, TLS/curl/subprocess disabled, and
builds only `llama-server-child`. It never patches input sources. Source hashes,
commands, compiler/toolchain identity, logs, PIC checks and ELF inspection are
saved beside the actual SO. PIC verification selects every object in the SO's
transitive Ninja input graph and requires complete compile-command coverage;
unrelated executable entries in the database do not qualify or disqualify this
SO. The verifier records the graph, object hashes, static archives, exports,
dynamic dependencies and the upstream crash-only process imports.
x86_64 build/ELF inspection is not device, ARM64,
actual-model or product lifecycle qualification. Keep upstream/vendor licenses
when packaging the SO. Product packaging, host controller and device acceptance
belong to the integrator.
