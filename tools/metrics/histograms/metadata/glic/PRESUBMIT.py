# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for tools/metrics/histograms/metadata/glic."""

PRESUBMIT_VERSION = '2.0.0'

_HISTOGRAMS_PATH = 'tools/metrics/histograms/metadata/glic/histograms.xml'


def CheckGlicMetrics(input_api, output_api):
  """Runs check_glic_metrics.py when the Glic histograms.xml changes.

  Runs the XML check, plus the C++ scan so that XML-only changes still
  check C++ callers. If the change also touches chrome/browser/glic, that
  directory's PRESUBMIT.py already runs the C++ scan, so it is skipped here
  to avoid reporting each error twice.
  """
  histograms_affected = False
  glic_dir_affected = False
  for f in input_api.change.AffectedFiles(include_deletes=False):
    path = f.LocalPath().replace('\\', '/')
    histograms_affected |= path == _HISTOGRAMS_PATH
    glic_dir_affected |= path.startswith('chrome/browser/glic/')

  if not histograms_affected:
    return []

  checks = ['xml'] if glic_dir_affected else ['cpp', 'xml']
  old_path = input_api.sys.path[:]
  try:
    tools_path = input_api.os_path.join(
      input_api.change.RepositoryRoot(), 'chrome', 'browser', 'glic', 'tools'
    )
    input_api.sys.path.insert(0, tools_path)
    import glic_metrics_presubmit_runner  # type: ignore[import-not-found]
    return glic_metrics_presubmit_runner.RunGlicMetricsCheck(
      input_api, output_api, checks
    )
  finally:
    input_api.sys.path = old_path
