"""Isolated regex owners: strict C23, optimized and ASan/UBSan execution.

Builds real regex units and native Memory/SpinLock dependencies in temporary
backing, outside shared build state. Children have finite watchdogs. This does
not prove TSan, injected allocation failure, or complete backend pressure.
"""
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parents[3]
    source = root / "suite/host-behavior/vexspoke/src"
    engine = root / "suite/vex/re/src"
    common = [os.environ.get("CC", "cc"), "-std=gnu23", "-Wall", "-Wextra",
              "-Werror", "-UNDEBUG", "-I", str(source), "-I", str(engine)]
    if os.uname().sysname == "Darwin":
        common += ["-arch", "arm64", "-mmacosx-version-min=14.0", "-mcpu=apple-m1"]
    units = sorted((source / "regex").glob("*.c"))
    units += [engine / "nio/mem.c", source / "atomic/spin.c"]
    owners = sorted(Path(__file__).parent.glob("*_test.c"))
    with tempfile.TemporaryDirectory(prefix="regex-owner-") as scratch:
        scratch = Path(scratch)
        for mode, flags in [("optimized", ["-O2"]),
                            ("sanitized", ["-O1", "-g", "-fsanitize=address,undefined",
                                           "-fno-omit-frame-pointer"])]:
            objects = []
            for index, unit in enumerate(units):
                obj = scratch / f"{mode}-{index}.o"
                subprocess.run(common + flags + ["-c", str(unit), "-o", str(obj)],
                               check=True, timeout=120)
                objects.append(str(obj))
            for owner in owners:
                binary = scratch / f"{mode}-{owner.stem}"
                subprocess.run(common + flags + [str(owner), *objects, "-lpthread",
                                                "-o", str(binary)],
                               check=True, timeout=120)
                env = dict(os.environ, UBSAN_OPTIONS="halt_on_error=1")
                subprocess.run([str(binary)], check=True, timeout=30, env=env)
                print(f"PASS {mode}: {owner.name}", flush=True)


if __name__ == "__main__":
    main()
