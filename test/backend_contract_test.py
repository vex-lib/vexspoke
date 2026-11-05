"""Default production ownership/provenance, not complete allocator readiness.

Builds the actual default consumer closure; verifies archive symbols, metadata,
canonical include resolution and registered owner execution. No windows launched.
"""
from pathlib import Path
import json
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
VEX = ROOT / "host-behavior/vexspoke"
ENGINE = ROOT / "vex/re"


def run(*args):
    return subprocess.run(args, cwd=ROOT, check=True, capture_output=True,
                          text=True, timeout=180).stdout


class BackendContractTest(unittest.TestCase):
    def test_no_duplicate_storage_sources_or_headers(self):
        for part in ("io", "nio"):
            self.assertFalse(list((VEX / "src" / part).glob("*")))
            self.assertTrue(list((ENGINE / "src" / part).glob("*.h")))
        self.assertFalse((VEX / "src/objc/clipboard_mac.m").exists())
        self.assertTrue((ENGINE / "src/io/clipboard_mac.m").is_file())

    def test_default_build_and_archive_provenance(self):
        run("./tools/b", "build", "darling")
        metadata = json.loads(run("./tools/b", "ide", "--json"))
        targets = {target["name"]: target for target in metadata["index"]}
        engine = targets["relational_engine"]
        expected = {str(p) for part in ("nio", "io")
                    for p in (ENGINE / "src" / part).glob("*")
                    if p.suffix in (".c", ".m")}
        self.assertEqual(set(engine["sources"]), expected)
        for name in ("vexspoke", "graphvex", "hotcwap", "darling"):
            self.assertIn(str(ENGINE / "src"), targets[name]["includes"])
        cpu = targets["vexspoke"]
        self.assertFalse(any("/src/io/" in p or "/src/nio/" in p for p in cpu["sources"]))
        owners = {target["name"]: target for target in metadata["tests"]}
        libs = owners["mem_test"]["libraries"]
        for suffix, present in (("librelational_engine.a", True), ("libvexspoke.a", False)):
            archive = next(p for p in libs if p.endswith(suffix))
            symbols = run("nm", "-gU", archive).splitlines()
            matches = [line for line in symbols if line.split()[-1:] == ["_Memory_alloc"]]
            self.assertEqual(len(matches), int(present), archive)
        for name in ("mem_test", "cache_test", "process_spawn_test", "frame_inbox_test"):
            self.assertIn("re/test/", owners[name]["sources"][0])
            run("./tools/b", "test", name)
        header = run("clang", "-std=gnu23", "-E", "-H", "-I" + str(VEX / "src"),
                     "-I" + str(ENGINE / "src"), "-x", "c", str(ENGINE / "src/nio/mem.h"))
        self.assertIn("Memory_alloc", header)

    def test_documented_native_not_rust_rewrite(self):
        content = (VEX / "BACKEND.md").read_text()
        for phrase in ("no IO/NIO copies", "default", "not a Rust", "R1 owns", "staged"):
            self.assertIn(phrase, content)
        self.assertIn("relational_engine/memory.h",
                      (ENGINE / "src/nio/relational_memory.h").read_text())


if __name__ == "__main__":
    unittest.main(verbosity=2)
