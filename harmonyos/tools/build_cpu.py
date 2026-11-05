#!/usr/bin/env python3
"""Build standalone HarmonyOS CPU tools from this fork, without Ollama or NPU."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

SOURCE = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sdk', type=Path, required=True, help='OpenHarmony native SDK')
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--arch', choices=['x86_64', 'arm64'], default='x86_64')
    p.add_argument('--jobs', type=int, default=4)
    p.add_argument('--native-child', action='store_true')
    p.add_argument('--configure-only', action='store_true')
    a = p.parse_args()
    a.sdk = a.sdk.resolve()
    a.out = a.out.resolve()
    if a.out.exists():
        p.error('--out must be a new directory')
    a.out.mkdir(parents=True)
    build = a.out / 'build'
    abi = 'arm64-v8a' if a.arch == 'arm64' else a.arch
    command = ['cmake', '-S', str(SOURCE), '-B', str(build), '-G', ('Ninja' if shutil.which('ninja') else 'Unix Makefiles'),
        '-DCMAKE_TOOLCHAIN_FILE=' + str(a.sdk / 'build/cmake/ohos.toolchain.cmake'),
        '-DOHOS_ARCH=' + abi, '-DOHOS_STL=c++_static', '-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_POSITION_INDEPENDENT_CODE=ON', '-DBUILD_SHARED_LIBS=OFF',
        '-DGGML_OPENMP=OFF', '-DGGML_NATIVE=OFF', '-DLLAMA_CURL=OFF', '-DLLAMA_OPENSSL=OFF',
        '-DLLAMA_SUBPROCESS=OFF', '-DLLAMA_BUILD_TESTS=ON', '-DLLAMA_BUILD_TOOLS=ON',
        '-DLLAMA_BUILD_SERVER=ON', '-DLLAMA_BUILD_EXAMPLES=ON', '-DLLAMA_BUILD_APP=OFF',
        '-DLLAMA_BUILD_UI=OFF', '-DLLAMA_BUILD_HTML=OFF', '-DCMAKE_C_FLAGS=-UNDEBUG', '-DCMAKE_CXX_FLAGS=-UNDEBUG']
    if a.arch == 'x86_64':
        command += ['-DGGML_' + f + '=OFF' for f in ['AVX', 'AVX2', 'FMA', 'F16C', 'SSE42']]
    if a.native_child:
        command += ['-DCMAKE_PROJECT_INCLUDE=' + str(SOURCE / 'harmonyos/native-server/native_server.cmake')]
    commands = [command]
    if not a.configure_only:
        targets = ['llama-cli', 'llama-server', 'llama-bench', 'llama-quantize', 'test-rope', 'test-quantize-fns']
        if a.native_child:
            targets.append('llama-server-child')
        commands.append(['cmake', '--build', str(build), '-j', str(a.jobs), '--target', *targets])
    record = {'arch': a.arch, 'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=SOURCE, text=True).strip(),
        'source_diff_sha256': hashlib.sha256(subprocess.check_output(['git', 'diff', 'HEAD'], cwd=SOURCE)).hexdigest(),
        'toolchain_sha256': hashlib.sha256((a.sdk / 'build/cmake/ohos.toolchain.cmake').read_bytes()).hexdigest(),
        'commands': [], 'built': False, 'device_qualified': False, 'tls': False, 'router': False}
    try:
        for i, command in enumerate(commands):
            with (a.out / ('command-' + str(i) + '.log')).open('w') as log:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
            record['commands'].append({'argv': command, 'exit': result.returncode})
            if result.returncode:
                raise RuntimeError('Build failed; see command log')
        if not a.configure_only:
            sdk = a.out / 'sdk'
            for name in ['include', 'lib']:
                (sdk / name).mkdir(parents=True)
            for f in [SOURCE / 'include/llama.h', *(SOURCE / 'ggml/include').glob('*.h')]:
                shutil.copy2(f, sdk / 'include' / f.name)
            for f in [build / 'src/libllama.a', *(build / 'ggml/src').glob('libggml*.a')]:
                shutil.copy2(f, sdk / 'lib' / f.name)
            record['built'] = True
            record['binaries'] = {str(f.relative_to(build)): hashlib.sha256(f.read_bytes()).hexdigest()
                for f in (build / 'bin').iterdir() if f.is_file()}
    finally:
        (a.out / 'identity.json').write_text(json.dumps(record, indent=2) + '\n')
    print(a.out / 'identity.json')

if __name__ == '__main__':
    main()
