"""Opt-in HDC CLI startup check; no models or application permission claims."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import uuid

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hdc', required=True)
    p.add_argument('--target', required=True)
    p.add_argument('--binary', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=False)
    token = uuid.uuid4().hex
    remote = '/data/local/tmp/llama-harmony-smoke-' + token
    marker = 'LLAMA_SMOKE_EXIT_' + token + '='
    record = {'passed':False, 'cleanup':False, 'model_loaded':False}
    owned = False
    def hdc(*argv):
        run = subprocess.run([a.hdc, '-t', a.target, *argv], capture_output=True, text=True, timeout=60)
        if run.returncode or '[Fail]' in run.stdout:
            raise RuntimeError('HDC failed: ' + run.stdout + run.stderr)
        return run.stdout
    try:
        size = a.binary.stat().st_size
        if not 20 <= size <= 256*1024*1024:
            raise ValueError('Expected a bounded native ELF binary')
        header = a.binary.read_bytes()[:20]
        if header[:6] != b'\x7fELF\x02\x01':
            raise ValueError('Expected a 64-bit little-endian ELF')
        machine = int.from_bytes(header[18:20], 'little')
        arch = hdc('shell', 'uname -m').strip()
        if (arch, machine) not in [('x86_64',62),('aarch64',183)]:
            raise ValueError('Binary does not match target architecture')
        digest = hashlib.sha256(a.binary.read_bytes()).hexdigest()
        hdc('shell', 'mkdir ' + shlex.quote(remote))
        owned = True
        hdc('file', 'send', str(a.binary.resolve()), remote+'/llama-cli')
        actual = hdc('shell', 'sha256sum ' + shlex.quote(remote+'/llama-cli')).split()
        if not actual or actual[0] != digest:
            raise ValueError('Transferred binary hash differs')
        executable = shlex.quote(remote+'/llama-cli')
        output = hdc('shell', 'chmod 700 '+executable+' && '+executable+' --help; rc=$?; printf "\\n'+marker+'%s\\n" "$rc"')
        (a.out/'stdout.txt').write_text(output)
        codes = re.findall(r'^'+re.escape(marker)+r'(\d+)\s*$', output, re.M)
        if codes != ['0'] or '--help' not in output:
            raise RuntimeError('CLI did not report a successful help invocation')
        record.update(passed=True, binary_sha256=digest, architecture=arch)
    except Exception as e:
        record['error'] = str(e)
    finally:
        if owned:
            try:
                hdc('shell', 'rm -rf ' + shlex.quote(remote))
                record['cleanup'] = True
            except Exception as e:
                record['cleanup_error'] = str(e)
                record['passed'] = False
        (a.out/'result.json').write_text(json.dumps(record, indent=2)+'\n')
    raise SystemExit(0 if record['passed'] else 1)

if __name__ == '__main__':
    main()
