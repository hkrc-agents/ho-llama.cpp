"""Host subprocess checks with ABI/server stubs; not OHOS runtime qualification."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
RUNNER = HERE.parents[2]


class EntryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="native-child-host-")
        cls.exe = Path(cls.temp.name) / "entry-test"
        subprocess.run(["c++", "-std=c++17", "-D__OHOS__", "-I" + str(HERE),
                        "-I" + str(RUNNER / "vendor"), str(HERE.parent / "entry.cpp"),
                        str(HERE / "server_stub.cpp"), "-o", str(cls.exe)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.run_dir = tempfile.TemporaryDirectory(dir=self.temp.name)
        self.addCleanup(self.run_dir.cleanup)
        self.log = Path(self.run_dir.name) / "server.log"
        self.receipt = Path(self.run_dir.name) / "receipt.jsonl"
        self.params = dict(schema=1, argv=[], log_path=str(self.log), receipt_path=str(self.receipt))

    def call(self, params=None):
        return subprocess.run([str(self.exe), json.dumps(self.params if params is None else params)],
                              capture_output=True, timeout=10)

    def test_literal_argv_limits_receipts_and_return_code(self):
        values = ["space path", 'a"b', "$(touch should-not-run)", "x\ny", "中文🙂"]
        self.params["argv"] = values
        result = self.call()
        self.assertEqual(result.returncode, 7, result.stderr)
        records = [json.loads(line) for line in self.receipt.read_text().splitlines()]
        self.assertEqual([v["phase"] for v in records], ["enter", "return"])
        self.assertEqual(records[-1]["rc"], 7)
        for item in records:
            self.assertGreater(item["pid"], 0)
            self.assertEqual(item["address_space_policy"], "not-set")
        line = next(v for v in self.log.read_text().splitlines() if v.startswith("STUB_ARGV "))
        self.assertEqual(json.loads(line[10:]), ["llama-server"] + values)

    def test_invalid_arguments_fail_before_server(self):
        for values in [["a\0b"], [42], ["x"] * 257, "--port 18189"]:
            with self.subTest(values=str(values)[:50]):
                self.params["argv"] = values
                self.assertEqual(self.call().returncode, 125)
                self.assertFalse(self.log.exists())

    def test_wrong_schema_unknown_keys_and_oversize(self):
        for change in [dict(schema=2), dict(extra=True), dict(argv=["x" * 65536])]:
            self.assertEqual(self.call(self.params | change).returncode, 125)
            self.assertFalse(self.log.exists())

    def test_malformed_json_and_missing_fields(self):
        for value in ["{", "[]", "null", '{"schema":1}', '{"schema":1,"argv":[' ]:
            with self.subTest(value=value):
                result = subprocess.run([str(self.exe), value], capture_output=True, timeout=10)
                self.assertEqual(result.returncode, 125)
                self.assertFalse(self.log.exists())

    def test_no_virtual_address_cap_is_installed(self):
        result = self.call()
        self.assertEqual(result.returncode, 7, result.stderr)
        for record in map(json.loads, self.receipt.read_text().splitlines()):
            self.assertEqual(record["address_space_policy"], "not-set")
            self.assertNotIn("rlimit_as_soft", record)

    def test_existing_file_is_preserved(self):
        self.log.write_text("keep me")
        self.assertEqual(self.call().returncode, 125)
        self.assertEqual(self.log.read_text(), "keep me")
        self.assertEqual(json.loads(self.receipt.read_text())["phase"], "error")

    def test_symlink_is_rejected(self):
        target = Path(self.run_dir.name) / "target"
        target.write_text("keep target")
        self.log.symlink_to(target)
        self.assertEqual(self.call().returncode, 125)
        self.assertEqual(target.read_text(), "keep target")

    def test_relative_and_equal_paths_are_rejected(self):
        self.params["log_path"] = "relative.log"
        self.assertEqual(self.call().returncode, 125)
        self.params["log_path"] = str(self.receipt)
        self.assertEqual(self.call().returncode, 125)


if __name__ == "__main__":
    unittest.main()
