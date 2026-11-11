#!/usr/bin/env python3
"""Run CMake with a retained log and bounded GitHub compiler annotations.

Annotations keep actual compiler failures visible through the checks API, even
when CI log storage is on a host unavailable to a restricted build agent. This
is a build wrapper, not a replacement for compilation or gameplay tests.
"""

import argparse
from collections import deque
import os
from pathlib import Path
import re
import subprocess
import sys


ERROR = re.compile(r"(?:: (?:fatal )?error\b|\berror (?:C|LNK|MSB)\d+\b|undefined reference|CMake Error|^FAILED:)", re.IGNORECASE)


def annotation(message):
    if os.environ.get("GITHUB_ACTIONS") == "true":
        escaped = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
        print("::error::" + escaped, flush=True)


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    if not command:
        parser.error("Missing build command")
    args.log.parent.mkdir(parents=True, exist_ok=True)
    tail, errors = deque(maxlen=12), []
    # Windows tool output encoding can differ from the Python locale. Compiler
    # identifiers/exit codes are ASCII; preserve other bytes with replacement.
    with args.log.open("w", encoding="utf-8") as log, subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace") as process:
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
            tail.append(line.rstrip())
            if len(errors) < 25 and ERROR.search(line):
                errors.append(line.rstrip())
        result = process.wait()
    if result:
        for line in errors or tail:
            annotation(line[:3000])
        annotation(f"Build command exited {result}; complete log: {args.log.as_posix()}")
    return result


if __name__ == "__main__":
    sys.exit(main())
