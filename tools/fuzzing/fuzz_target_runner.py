#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs one fuzzing session of a Chromium fuzz target and records it.

Note: The target must already be built in --build-dir. The runner never runs GN
or ninja.

Example:
  python3 tools/fuzzing/fuzz_target_runner.py \\
    --engine=libfuzzer --build-dir=out/libfuzzer-asan \\
    --target=zlib_inflate_fuzzer --duration=15m \\
    --output-dir=/tmp/pb/zlib/libfuzzer/1

Planned output directory:
  trial.json      Inputs: target, engine and options, command and env, host.
  summary.json    Results: exit reason, engine exit code and wall time.
  logs/           engine.log, the engine's combined stdout and stderr.
  corpus/ crashes/ work/ home/

Exit codes:
  0    The session ran; read summary.json.
  1    A runner bug (an uncaught Python exception).
  2    Bad flags, build dir or output dir.
  3    The target isn't built in the build dir.
  4    The engine didn't start.
  130  Interrupted by Ctrl-C or SIGTERM.
"""

from __future__ import annotations

import argparse
import dataclasses
import enum
import os
import pathlib
import re
import sys

import engines


class ExitCode(enum.IntEnum):
  """Process exit codes."""

  OK = 0
  # 1 is unused: Python exits with 1 on an uncaught exception, meaning a
  # runner bug.
  BAD_SPEC = 2  # Also what argparse exits with on bad flags
  MISSING_BINARY = 3
  ENGINE_DID_NOT_START = 4
  INTERRUPTED = 130


class SetupError(Exception):
  """A problem with the build dir or output dir. Carries the exit code."""

  def __init__(self, message: str, exit_code: ExitCode = ExitCode.BAD_SPEC):
    super().__init__(message)
    self.exit_code = exit_code


@dataclasses.dataclass(frozen=True)
class Layout:
  """The directory paths within the output directory."""

  root: pathlib.Path

  @property
  def corpus(self) -> pathlib.Path:
    return self.root / 'corpus'

  @property
  def crashes(self) -> pathlib.Path:
    return self.root / 'crashes'

  # Engine cwd, keeping relative outputs inside the output dir.
  @property
  def work(self) -> pathlib.Path:
    return self.root / 'work'

  # The engine's $HOME, so a host's state doesn't affect fuzzers.
  @property
  def home(self) -> pathlib.Path:
    return self.root / 'home'

  @property
  def logs(self) -> pathlib.Path:
    return self.root / 'logs'

  def subdirs(self) -> list[pathlib.Path]:
    return [self.corpus, self.crashes, self.work, self.home, self.logs]


def parse_duration(value: str) -> int:
  """Parses '900', '900s', '15m', '1h' or '1h30m' into seconds."""
  value = value.strip().lower()
  if value.isdecimal():
    duration = int(value)
  else:
    parts = re.findall(r'(\d+)([hms])', value)
    if not parts or ''.join(num + unit for num, unit in parts) != value:
      raise argparse.ArgumentTypeError('bad duration: %r' % value)
    units = {'h': 3600, 'm': 60, 's': 1}
    duration = sum(int(num) * units[unit] for num, unit in parts)
  if duration <= 0:
    raise argparse.ArgumentTypeError('duration must be positive: %r' % value)
  return duration


def parse_nonempty(value: str) -> str:
  value = value.strip()
  if not value:
    raise argparse.ArgumentTypeError('must not be empty')
  return value


def parse_path(value: str) -> pathlib.Path:
  """Makes the path absolute, because the engine runs with its cwd inside the
  output dir."""
  return pathlib.Path(os.path.abspath(parse_nonempty(value)))


def session_from_argv(argv: list[str] | None) -> Session:
  """Parses command line arguments into a Session."""
  parser = argparse.ArgumentParser(
    description=__doc__,
    formatter_class=argparse.RawDescriptionHelpFormatter,
    allow_abbrev=False,
  )
  parser.add_argument('--engine', required=True, choices=engines.ENGINES)
  parser.add_argument(
    '--build-dir',
    required=True,
    type=parse_path,
    help='GN out dir where the target is built.',
  )
  parser.add_argument(
    '--target', required=True, type=parse_nonempty, help='Fuzz target name.'
  )
  parser.add_argument(
    '--duration',
    required=True,
    type=parse_duration,
    help='How long the engine is told to fuzz, e.g. 900, 15m, 1h.',
  )
  parser.add_argument(
    '--output-dir',
    required=True,
    type=parse_path,
    help='Must be missing or empty.',
  )
  parser.add_argument(
    '--seed',
    type=int,
    help='Engine RNG seed. If unset, the engine picks a random one.',
  )
  # TODO(crbug.com/581948942): Add flags for the engine limits in EngineOptions.
  args = parser.parse_args(argv)

  return Session(
    adapter=engines.get_adapter(args.engine),
    target=engines.Target(build_dir=args.build_dir, name=args.target),
    options=engines.EngineOptions(duration_s=args.duration, seed=args.seed),
    layout=Layout(args.output_dir),
  )


@dataclasses.dataclass(frozen=True)
class Session:
  """Runs one fuzz target under one engine and records it in the output dir."""

  adapter: engines.EngineAdapter
  target: engines.Target
  options: engines.EngineOptions
  layout: Layout
  # TODO(crbug.com/581948942): Add a hard-deadline timeout.

  def run(self) -> ExitCode:
    """Runs the session."""
    # TODO(crbug.com/581948942): Validate build/output dirs, run the engine
    # via self.adapter.command(), and record trial.json, summary.json, and logs.
    raise NotImplementedError('fuzz_target_runner.py is a skeleton')


def main(argv: list[str] | None = None) -> ExitCode:
  try:
    return session_from_argv(argv).run()
  except SetupError as e:
    print('error: %s' % e, file=sys.stderr)
    return e.exit_code


if __name__ == '__main__':
  sys.exit(main())
