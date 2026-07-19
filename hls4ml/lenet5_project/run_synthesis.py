#!/usr/bin/env python3
"""Start Vitis HLS synthesis (build_prj.tcl) with live progress output."""
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parent
LOG_FILE = PROJECT_DIR / "synthesis.log"


def main():
    start = time.time()
    print(f"[run_synthesis] starting vitis_hls -f build_prj.tcl in {PROJECT_DIR}")
    print(f"[run_synthesis] logging to {LOG_FILE}")

    with open(LOG_FILE, "a") as log:
        log.write(f"\n===== run started {datetime.now().isoformat()} =====\n")
        log.flush()

        proc = subprocess.Popen(
            ["vitis_hls", "-f", "build_prj.tcl"],
            cwd=PROJECT_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        for line in proc.stdout:
            sys.stdout.write(line)
            sys.stdout.flush()
            log.write(line)
            log.flush()
        returncode = proc.wait()

        elapsed = time.time() - start
        h, rem = divmod(int(elapsed), 3600)
        m, s = divmod(rem, 60)
        summary = f"[run_synthesis] finished after {h}h{m}m{s}s, exit code {returncode}"
        print(summary)
        log.write(summary + "\n")

    sys.exit(returncode)


if __name__ == "__main__":
    main()
