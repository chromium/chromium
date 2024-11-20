#!/usr/bin/env python3

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, IO, NamedTuple


def eprint(*args: Any, **kwargs: Any) -> None:
    print(*args, end="\n", file=sys.stderr, flush=True, **kwargs)


def run_buildscript(
    buildscript: str,
    env: Dict[str, str],
    cwd: Path,
) -> str:
    try:
        return subprocess.check_output(
            os.path.abspath(buildscript),
            encoding="utf-8",
            env=env,
            cwd=cwd,
        )
    except OSError as ex:
        eprint(f"Failed to run {buildscript} because {ex}", file=sys.stderr)
        sys.exit(1)
    except subprocess.CalledProcessError as ex:
        sys.exit(ex.returncode)


class Args(NamedTuple):
    buildscript: str
    manifest_dir: Path
    rustc_wrapper: Path
    out_dir: Path
    rustc_args: IO[str]


def arg_parse() -> Args:
    parser = argparse.ArgumentParser(description="Run Rust build script")
    parser.add_argument("--buildscript", type=str, required=True)
    parser.add_argument("--manifest-dir", type=Path, required=True)
    parser.add_argument("--rustc-wrapper", type=Path, required=True)
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--rustc-args", type=argparse.FileType("w"), required=True)

    return Args(**vars(parser.parse_args()))


def main():
    args = arg_parse()

    env = dict(
        os.environ,
        CARGO_MANIFEST_DIR=os.path.abspath(args.manifest_dir),
        OUT_DIR=os.path.abspath(args.out_dir),
        RUSTC=os.path.abspath(args.rustc_wrapper),
    )

    script_output = run_buildscript(
        args.buildscript,
        env=env,
        cwd=args.manifest_dir,
    )

    cargo_rustc_cfg_pattern = re.compile("^cargo:rustc-cfg=(.*)")
    flags = ""
    for line in script_output.split("\n"):
        cargo_rustc_cfg_match = cargo_rustc_cfg_pattern.match(line)
        if cargo_rustc_cfg_match:
            flags += "--cfg={}\n".format(cargo_rustc_cfg_match.group(1))
    flags += "--env-set=OUT_DIR={}\n".format(os.path.abspath(args.out_dir))
    args.rustc_args.write(flags)


if __name__ == "__main__":
    main()
