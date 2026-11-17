# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Engine adapters for fuzz_target_runner.py."""

from __future__ import annotations

import dataclasses
import pathlib

# TODO(crbug.com/584810685): Add 'centipede' and 'auto' engine detection.
ENGINES = ('libfuzzer',)


def get_adapter(engine: str) -> EngineAdapter:
  """Returns a new adapter for an engine named in ENGINES."""
  if engine == 'libfuzzer':
    return LibFuzzerAdapter()
  raise ValueError('unknown engine: %s' % engine)


@dataclasses.dataclass(frozen=True)
class Target:
  """Which fuzz target to run: an already-built binary in a GN out dir."""

  build_dir: pathlib.Path
  name: str


@dataclasses.dataclass(frozen=True)
class EngineOptions:
  """Engine-neutral fuzzing settings."""

  # Fuzzing duration passed to the engine.
  duration_s: int
  # None means the engine picks a random seed for each run.
  seed: int | None = None
  # TODO(crbug.com/581948942): Add explicit engine resource and timeout limits.


class EngineAdapter:
  """Translates a Target and EngineOptions into one engine's command line."""

  name = ''

  def command(
    self,
    target: Target,
    options: EngineOptions,
    *,
    corpus_dir: pathlib.Path,
    crashes_dir: pathlib.Path,
  ) -> list[str]:
    """Returns the command line that runs the target under this engine.

    The binary is <target.build_dir>/<target.name>. The engine writes its corpus
    into corpus_dir and its crashes into crashes_dir, and stops by itself after
    options.duration_s.
    """
    raise NotImplementedError


class LibFuzzerAdapter(EngineAdapter):
  name = 'libfuzzer'

  # TODO(crbug.com/581948942): Implement command().
