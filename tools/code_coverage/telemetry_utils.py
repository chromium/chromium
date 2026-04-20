# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from opentelemetry import trace

# Appends third_party/depot_tools/infra_lib so that we can import telemetry.
sys.path.append(
    os.path.join(os.path.dirname(__file__), os.path.pardir, os.path.pardir,
                 'third_party', 'depot_tools', 'infra_lib'))

import telemetry

tracer = telemetry.get_tracer(__name__)


def Initialize():
  """Initializes telemetry."""
  telemetry.initialize('chromium.tools.code_coverage')


def RecordMainAttributes(targets, out_dir):
  """Records main attributes to the current span."""
  span = trace.get_current_span()
  if not span.is_recording():
    return

  if targets:
    span.set_attribute('main.targets', str(targets))
  if out_dir:
    span.set_attribute('build.out_dir', out_dir)
