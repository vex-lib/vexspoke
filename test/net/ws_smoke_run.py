#!/usr/bin/env python3
"""Owner build for the WebSocket smoke: idle cost, plus an opt-in live round-trip.

MODULE: ws_smoke_run.py
Always runs the offline idle-cost owner (a quiet session must not burn CPU).
The live tier runs only when WS_SMOKE_URL is set, because the shared suite must
never depend on the live internet; that tier is the only place a completed TLS
handshake and a foreign server's framing are proved. A known-good target that
needs no credentials is Discord's gateway, which sends HELLO unprompted:

    WS_SMOKE_URL='wss://gateway.discord.gg/?v=10&encoding=json' \\
        python3 vexspoke/test/net/ws_smoke_run.py
"""
from pathlib import Path
import os
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
CPU = ROOT / "suite/ecosystem/vexspoke"
ENGINE = ROOT / "suite/ecosystem/relational-engine"
TEST = HERE / "websocket_smoke_test.c"

SOURCES = [ENGINE / "src/nio/mem.c", CPU / "src/net/websocket.c",
           CPU / "src/net/tls_apple.m", CPU / "src/net/url.c",
           CPU / "src/security/sha1.c", CPU / "src/security/secure_random.c",
           CPU / "src/lang/str.c", CPU / "src/atomic/spin.c"]


def main():
    with tempfile.TemporaryDirectory(prefix="ws-smoke-", dir=os.environ.get("TMPDIR")) as temp:
        build = Path(temp)
        flags = ["-std=gnu23", "-Wall", "-Wextra", "-Werror", "-O2", "-UNDEBUG",
                 "-mcpu=apple-m1", "-mmacosx-version-min=14.0",
                 "-I" + str(CPU / "src"), "-I" + str(ENGINE / "src"),
                 "-I" + str(ROOT / "tests")]
        objects = []
        for index, source in enumerate(SOURCES):
            arc = ["-fobjc-arc"] if source.suffix == ".m" else []
            obj = build / f"{index}.o"
            subprocess.run(["clang", *flags, *arc, "-c", str(source), "-o", str(obj)],
                           check=True, timeout=120)
            objects.append(str(obj))
        binary = build / "websocket_smoke"
        subprocess.run(["clang", *flags, str(TEST), *objects, "-framework", "Foundation",
                        "-framework", "Network", "-o", str(binary)],
                       check=True, timeout=120)
        result = subprocess.run([str(binary)], timeout=300, capture_output=True, text=True)
        if result.stdout:
            print(result.stdout, end="")
        if result.returncode != 0:
            if result.stderr:
                print(result.stderr, end="", file=__import__("sys").stderr)
            raise AssertionError(f"WebSocket smoke failed (exit {result.returncode})")
        print("PASS: WebSocket smoke (idle cost"
              + (", live round-trip" if os.environ.get("WS_SMOKE_URL") else
                 "; live round-trip skipped, WS_SMOKE_URL unset")
              + (", discord gateway" if os.environ.get("WS_SMOKE_DISCORD") else
                 "; discord gateway skipped, WS_SMOKE_DISCORD unset") + ")")


if __name__ == "__main__":
    main()
