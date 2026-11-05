"""Host transport controls; these do not run a target binary."""
from pathlib import Path
import json
import os
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1]/'tools/device_smoke.py'
FAKE = '''#!/usr/bin/env python3
import hashlib,os,re,sys
args=sys.argv[3:]
with open(os.environ['SMOKE_COMMANDS'],'a') as f:f.write(repr(args)+'\\n')
if args==['shell','uname -m']:print('x86_64')
elif args[0]=='shell' and args[1].startswith('sha256sum'):
 print('bad' if os.environ.get('SMOKE_BAD_HASH') else hashlib.sha256(open(os.environ['SMOKE_BINARY'],'rb').read()).hexdigest())
elif args[0]=='shell' and 'printf' in args[1]:
 marker=re.search(r'LLAMA_SMOKE_EXIT_[a-f0-9]+=',args[1])[0]
 print('--help support\\n'+marker+os.environ.get('SMOKE_EXIT','0'))
'''
class DeviceSmokeControls(unittest.TestCase):
    def run_case(self, **options):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);binary=root/'binary';header=bytearray(20);header[:6]=b'\x7fELF\x02\x01';header[18:20]=(62).to_bytes(2,'little');binary.write_bytes(header)
            fake=root/'hdc';fake.write_text(FAKE);fake.chmod(0o700)
            env=dict(os.environ,SMOKE_BINARY=str(binary),SMOKE_COMMANDS=str(root/'commands'),**options)
            run=subprocess.run([sys.executable,str(SCRIPT),'--hdc',str(fake),'--target','test-target','--binary',str(binary),'--out',str(root/'out')],env=env,capture_output=True,text=True,timeout=15)
            return run.returncode,json.loads((root/'out/result.json').read_text()),(root/'commands').read_text()
    def test_success_requires_binary_ack_and_cleanup(self):
        code,result,commands=self.run_case();self.assertEqual(code,0);self.assertTrue(result['passed']);self.assertTrue(result['cleanup']);self.assertIn('rm -rf',commands)
    def test_zero_hdc_exit_does_not_hide_target_failure(self):
        code,result,_=self.run_case(SMOKE_EXIT='7');self.assertNotEqual(code,0);self.assertFalse(result['passed']);self.assertTrue(result['cleanup'])
    def test_hash_mismatch_never_invokes_binary(self):
        code,result,commands=self.run_case(SMOKE_BAD_HASH='1');self.assertNotEqual(code,0);self.assertTrue(result['cleanup']);self.assertNotIn('printf',commands)

if __name__=='__main__':unittest.main()
