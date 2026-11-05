"""Build this self-contained HAP with explicit tools and prebuilt CPU libraries.

No pico root, network experiment, DDK, or Ollama HAP template is required.
This does not build or qualify the supplied llama archives.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

EXAMPLE = Path(__file__).resolve().parent
SDK = EXAMPLE.parent / 'sdk'
SKIP = {'build', 'oh_modules', 'node_modules', '.hvigor', '.cxx', '.git', '__pycache__'}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def maintained(folder):
    return sorted(p for p in folder.rglob('*') if p.is_file()
                  and not any(part in SKIP for part in p.relative_to(folder).parts))


def dependencies(source, libraries):
    files = maintained(EXAMPLE) + maintained(SDK)
    files += [source / 'include/llama.h', source / 'ggml/include/ggml.h']
    files += [libraries / name for name in ('src/libllama.a',
              'ggml/src/libggml.a', 'ggml/src/libggml-base.a', 'ggml/src/libggml-cpu.a')]
    helper = libraries / 'vendor/hash/libvendor-hash.a'
    if helper.exists():
        files.append(helper)
    return {str(p.resolve()): sha(p) for p in files}


def stage(out):
    """Copy only this maintained example and its sibling SDK, never an experiment."""
    for folder, target in [(EXAMPLE, out / 'app'), (SDK, out / 'sdk')]:
        for path in maintained(folder):
            dest = target / path.relative_to(folder)
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dest)
    module = json.loads((out / 'app/entry/src/main/module.json5').read_text())['module']
    if module.get('requestPermissions') or module.get('hnpPackages'):
        raise ValueError('SDK example must not inherit network permissions or HNP packages')
    return out / 'app'


def configure_architecture(app,arch):
    abi,machine={'x86_64':('x86_64',62),'arm64':('arm64-v8a',183)}[arch]
    path=app/'entry/build-profile.json5'
    config=json.loads(path.read_text())
    config['buildOption']['externalNativeOptions']['abiFilters']=[abi]
    path.write_text(json.dumps(config,indent=2)+'\n')
    return machine


def own_output(out, identity, resume):
    """A retry may only update this builder's own isolated staging directory."""
    marker = out / 'sdk-build-owner.json'
    if resume:
        if not marker.is_file() or json.loads(marker.read_text()) != identity:
            raise ValueError('Resume requires a matching SDK build ownership marker')
    else:
        with marker.open('x') as stream:
            json.dump(identity, stream, indent=2)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'libraries', 'tools', 'java', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--expected-commit', required=True)
    p.add_argument('--sdk-root', type=Path, help='SDK root containing default/; otherwise use the tools sibling sdk/')
    p.add_argument('--lock-dir', type=Path, default=Path.home() / '.cache/ho-llama.cpp')
    p.add_argument('--resume', action='store_true')
    p.add_argument('--arch', choices=['x86_64','arm64'], default='x86_64')
    a = p.parse_args()
    for name in ('source', 'libraries', 'tools', 'java', 'out'):
        setattr(a, name, getattr(a, name).resolve())
    for protected in (EXAMPLE, SDK, a.source, a.libraries):
        if a.out == protected or a.out in protected.parents or protected in a.out.parents:
            raise SystemExit('Output must be outside source and library trees')
    a.out.mkdir(parents=True, exist_ok=a.resume)
    a.lock_dir.mkdir(parents=True, exist_ok=True)
    report = {'pass': False, 'commands': [], 'permissions': [], 'hnp': False,
              'scope': 'Self-contained downstream '+a.arch+' CPU SDK HAP build; not runtime qualification'}
    try:
        with (a.lock_dir / 'runtime-build.lock').open('a') as runtime_lock, (a.lock_dir / 'hap-build.lock').open('a') as hap_lock:
            fcntl.flock(runtime_lock, fcntl.LOCK_EX)
            fcntl.flock(hap_lock, fcntl.LOCK_EX)
            commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=a.source, text=True).strip()
            if commit != a.expected_commit:
                raise ValueError('Unexpected llama source revision')
            owner={'schema': 1, 'source': str(a.source), 'libraries': str(a.libraries),
                   'commit': commit, 'example': str(EXAMPLE)}
            if a.arch!='x86_64':owner['architecture']=a.arch
            own_output(a.out,owner,a.resume)
            before = dependencies(a.source, a.libraries)
            report.update(runner_commit=commit, dependencies=before)
            app = stage(a.out)
            machine=configure_architecture(app,a.arch)
            env = dict(os.environ, JAVA_HOME=str(a.java), PATH=str(a.java / 'bin') + ':' + os.environ['PATH'],
                       PICO_LLAMA_SOURCE=str(a.source), PICO_LLAMA_BUILD=str(a.libraries),
                       PICO_SDK_EXAMPLE_SOURCE=str(a.out / 'sdk'))
            sdk_root = (a.sdk_root or a.tools.parent / 'sdk').resolve()
            if not (sdk_root / 'default').is_dir():
                raise ValueError('SDK root must contain default/; pass --sdk-root explicitly')
            env['DEVECO_SDK_HOME'] = str(sdk_root)
            report['sdk_root'] = str(sdk_root)
            for name, command in [('ohpm', [str(a.tools / 'ohpm'), 'install']),
                                  ('hvigor', [str(a.tools / 'hvigorw'), '--no-daemon', 'assembleHap'])]:
                with (a.out / (name + '.log')).open('wb') as log:
                    result = subprocess.run(command, cwd=app, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=1200)
                report['commands'].append({'argv': command, 'exit': result.returncode})
                if result.returncode:
                    raise RuntimeError(name + ' failed; inspect the retained log')
            if before != dependencies(a.source, a.libraries):
                raise RuntimeError('SDK inputs changed while building')
            hap = a.out / 'sdk-example.hap'
            shutil.copy2(app / 'entry/build/default/outputs/default/entry-default-unsigned.hap', hap)
            with zipfile.ZipFile(hap) as archive:
                libraries=[name for name in archive.namelist() if name.endswith('.so')]
                if not libraries:raise ValueError('SDK HAP has no actual native library')
                for name in libraries:
                    header=archive.read(name)[:20]
                    if len(header)<20 or header[:6]!=b'\x7fELF\x02\x01' or int.from_bytes(header[18:20],'little')!=machine:
                        raise ValueError('Wrong SDK HAP native architecture')
            report.update({'pass': True, 'bundle': 'com.pico.sdksample', 'architecture': a.arch,
                           'hap_sha256': sha(hap), 'inputs_unchanged': True})
    except Exception as error:
        report['error'] = str(error)
    (a.out / 'identity.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'dependencies'}))
    raise SystemExit(0 if report['pass'] else 1)


if __name__ == '__main__':
    main()
