# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from core import perf_benchmark

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set
from contrib.cluster_telemetry import repaint_helpers

from telemetry import benchmark
from telemetry import story

from py_utils import discover

from measurements import skpicture_printer


def _MatchPageSetName(story_set_name, story_set_base_dir):
  story_sets = discover.DiscoverClasses(story_set_base_dir, story_set_base_dir,
                                        story.StorySet).values()
  for s in story_sets:
    if story_set_name == s.Name():
      return s
  return None


@benchmark.Info(emails=['rmistry@chromium.org'])
class SkpicturePrinter(perf_benchmark.PerfBenchmark):

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    parser.add_argument('--page-set-name')
    parser.add_argument('--page-set-base-dir')
    parser.add_argument('-s',
                        '--skp-outdir',
                        help='Output directory for the SKP files')

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    if not args.page_set_name:
      parser.error('Please specify --page-set-name')
    if not args.page_set_base_dir:
      parser.error('Please specify --page-set-base-dir')
    if not args.skp_outdir:
      parser.error('Please specify --skp-outdir')

  @classmethod
  def Name(cls):
    return 'skpicture_printer'

  def CreatePageTest(self, options):
    return skpicture_printer.SkpicturePrinter(options.skp_outdir)

  def CreateStorySet(self, options):
    story_set_class = _MatchPageSetName(options.page_set_name,
                                        options.page_set_base_dir)
    return story_set_class()


class SkpicturePrinterCT(perf_benchmark.PerfBenchmark):
  """Captures SKPs for Cluster Telemetry."""

  @classmethod
  def Name(cls):
    return 'skpicture_printer_ct'

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)
    parser.add_argument('-s',
                        '--skp-outdir',
                        help='Output directory for the SKP files')

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def CreatePageTest(self, options):
    return skpicture_printer.SkpicturePrinter(options.skp_outdir)

  def CreateStorySet(self, options):
    return page_set.CTPageSet(
        options.urls_list, options.user_agent, options.archive_data_file,
        run_page_interaction_callback=repaint_helpers.WaitThenRepaint)
