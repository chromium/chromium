# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# Forked from:
# https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/config/payload_utils/common/logging_utils.py
"""Provides a common functionality of managing the logging system"""

import argparse
import logging

_LOG_LEVELS = (
    "fatal",
    "critical",
    "error",
    "warning",
    "info",
    "debug",
)


def parser_add_argument(parser: argparse.ArgumentParser) -> None:
  """Add logging arguments to |parser|."""
  parser.add_argument(
      "-l",
      "--log-level",
      choices=_LOG_LEVELS,
      default="info",
      help="set logging level",
  )
  parser.add_argument(
      "-v",
      "--verbose",
      dest="log_level",
      action="store_const",
      const="info",
      help="Alias for `--log-level=info`.",
  )
  parser.add_argument(
      "--debug",
      dest="log_level",
      action="store_const",
      const="debug",
      help="Alias for `--log-level=debug`.",
  )


def name_to_value(name: str) -> int:
  """Convert logging |name| (e.g. "info") to logging level number."""
  if name.lower() not in _LOG_LEVELS:
    raise ValueError(f"Unknown log level: {name}")
  return getattr(logging, name.upper())


def config_logging(opts: argparse.Namespace) -> None:
  """Initialize logging based on the parsed namespace."""
  loglevel = name_to_value(opts.log_level)
  logging.basicConfig(
      level=loglevel,
      format="%(asctime)s %(levelname)-8s %(message)s",
      datefmt="%Y-%m-%dT%H:%M:%S",
  )
