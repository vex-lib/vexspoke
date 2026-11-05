#!/usr/bin/env python3
"""Owner build for the net/tls.h Apple backend (Network.framework).

MODULE: tls_apple_run.py
Compiles tls_apple.m with ARC and the C owner, then links Foundation + Network.
Only the refusal paths are exercised: a completed handshake needs an endpoint
whose chain validates against the system trust anchors, and this adapter has no
trust override to make a self-signed loopback peer stand in for one. The missing
positive proof is printed by the owner itself rather than implied by a pass.
"""
from pathlib import Path
import os
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CPU = ROOT / "suite/ecosystem/vexspoke"
ENGINE = ROOT / "suite/ecosystem/relational-engine"
TEST = HERE / "tls_apple_test.c"
IMPL = CPU / "src/net/tls_apple.m"

FRAMEWORKS = ["-framework", "Foundation", "-framework", "Network"]
MODES = [("strict", []), ("sanitized", ["-fsanitize=address,undefined"])]


def main():
    with tempfile.TemporaryDirectory(prefix="tls-apple-", dir=os.environ.get("TMPDIR")) as temp:
        build = Path(temp)
        for mode, sanitizer in MODES:
            flags = ["-std=gnu23", "-Wall", "-Wextra", "-Werror", "-O2", "-UNDEBUG",
                     "-fobjc-arc", "-mcpu=apple-m1", "-mmacosx-version-min=14.0",
                     *sanitizer, "-I" + str(CPU / "src"), "-I" + str(ENGINE / "src")]
            obj = build / f"{mode}-tls_apple.o"
            subprocess.run(["clang", *flags, "-c", str(IMPL), "-o", str(obj)],
                           check=True, timeout=180)
            binary = build / f"{mode}-tls_apple_test"
            subprocess.run(["clang", *flags, str(TEST), str(obj), *FRAMEWORKS,
                            "-o", str(binary)], check=True, timeout=180)
            result = subprocess.run([str(binary)], timeout=180, capture_output=True, text=True)
            if result.stdout:
                print(result.stdout, end="")
            if result.returncode != 0:
                if result.stderr:
                    print(result.stderr, end="", file=__import__("sys").stderr)
                raise AssertionError(f"TLS Apple owner failed in {mode} mode "
                                     f"(exit {result.returncode})")
        print("PASS: TLS Apple backend refusal-path owner (strict + sanitized)")


if __name__ == "__main__":
    main()
