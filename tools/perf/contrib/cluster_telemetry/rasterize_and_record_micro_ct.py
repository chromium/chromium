# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set
from contrib.cluster_telemetry import repaint_helpers

from benchmarks import rasterize_and_record_micro


# pylint: disable=protected-access
class RasterizeAndRecordMicroCT(
    rasterize_and_record_micro._RasterizeAndRecordMicro):
  """Measures rasterize and record performance for Cluster Telemetry."""

  @classmethod
  def Name(cls):
    return 'rasterize_and_record_micro_ct'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    (rasterize_and_record_micro._RasterizeAndRecordMicro.
        AddBenchmarkCommandLineArgs(parser))
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def CreateStorySet(self, options):
    return page_set.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file,
        run_page_interaction_callback=repaint_helpers.WaitThenRepaint)
