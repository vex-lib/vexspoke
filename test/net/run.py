#!/usr/bin/env python3
"""Fresh, bounded owner build for the Vexspoke WebSocket client.

MODULE: net/run.py
Builds the engine's native Memory ABI plus the Vexspoke protocol/security units
the client needs, then runs the forked-loopback owner under strict and
sanitized C23 with warnings denied. Generated inputs and binaries stay
disposable. The workspace graph remains independently blocked by the Vexspoke
ownership ratchet, so this runner is the registered evidence path for the
WebSocket seam.
"""
from pathlib import Path
import os
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SUITE = HERE.parent
ROOT = SUITE.parents[1]
CPU = ROOT / "suite/ecosystem/vexspoke"
ENGINE = ROOT / "suite/ecosystem/relational-engine"
TEST = HERE / "websocket_test.c"

SOURCES = [
    ENGINE / "src/nio/mem.c",
    CPU / "src/net/websocket.c",
    CPU / "src/net/tls_apple.m",
    CPU / "src/net/url.c",
    CPU / "src/security/sha1.c",
    CPU / "src/security/secure_random.c",
    CPU / "src/lang/str.c",
    CPU / "src/atomic/spin.c",
]

MODES = [("strict", []), ("sanitized", ["-fsanitize=address,undefined"])]


def main():
    with tempfile.TemporaryDirectory(prefix="ws-owner-", dir=os.environ.get("TMPDIR")) as temp:
        build = Path(temp)
        for mode, sanitizer in MODES:
            flags = ["-std=gnu23", "-Wall", "-Wextra", "-Werror", "-O2", "-UNDEBUG",
                     "-mcpu=apple-m1", "-mmacosx-version-min=14.0", *sanitizer,
                     "-I" + str(CPU / "src"), "-I" + str(ENGINE / "src"),
                     "-I" + str(ROOT / "tests")]
            objects = []
            for index, source in enumerate(SOURCES):
                arc = ["-fobjc-arc"] if source.suffix == ".m" else []
                obj = build / f"{mode}-{index}.o"
                subprocess.run(["clang", *flags, *arc, "-c", str(source), "-o", str(obj)],
                               check=True, timeout=120)
                objects.append(str(obj))
            binary = build / f"{mode}-websocket_test"
            subprocess.run(["clang", *flags, str(TEST), *objects, "-framework", "Foundation",
                            "-framework", "Network", "-o", str(binary)],
                           check=True, timeout=120)
            result = subprocess.run([str(binary)], timeout=120, capture_output=True, text=True)
            if result.stdout:
                print(result.stdout, end="")
            if result.returncode != 0:
                if result.stderr:
                    print(result.stderr, end="", file=__import__("sys").stderr)
                raise AssertionError(f"WebSocket owner failed in {mode} mode "
                                     f"(exit {result.returncode})")
        print("PASS: strict + ASan/UBSan WebSocket owner")


if __name__ == "__main__":
    main()
