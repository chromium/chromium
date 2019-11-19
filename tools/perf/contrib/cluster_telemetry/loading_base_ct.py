# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from benchmarks import loading

from contrib.cluster_telemetry import ct_benchmarks_util
from contrib.cluster_telemetry import page_set

from telemetry.page import cache_temperature as cache_temperature_module
from telemetry.page import traffic_setting

# pylint: disable=protected-access
class _LoadingBaseClusterTelemetry(loading._LoadingBase):
  """ A base class for cluster telemetry loading benchmarks. """

  options = {'upload_results': True}

  _ALL_NET_CONFIGS = traffic_setting.NETWORK_CONFIGS.keys()
  _ALL_CACHE_TEMPERATURES = cache_temperature_module.ALL_CACHE_TEMPERATURES

  @classmethod
  def AddBenchmarkCommandLineArgs(cls, parser):
    super(_LoadingBaseClusterTelemetry, cls).AddBenchmarkCommandLineArgs(parser)
    ct_benchmarks_util.AddBenchmarkCommandLineArgs(parser)
    parser.add_option(
        '--wait-time',  action='store', type='int',
        default=60, help='Number of seconds to wait for after navigation.')
    parser.add_option(
        '--traffic-setting',  choices=cls._ALL_NET_CONFIGS,
        default=traffic_setting.REGULAR_4G,
        help='Traffic condition (string). Default to "%%default". Can be: %s' %
         ', '.join(cls._ALL_NET_CONFIGS))
    parser.add_option(
        '--cache-temperature',  choices=cls._ALL_CACHE_TEMPERATURES,
        default=cache_temperature_module.COLD,
        help='Cache temperature (string). Default to "%%default". Can be: %s' %
         ', '.join(cls._ALL_CACHE_TEMPERATURES))

  @classmethod
  def ProcessCommandLineArgs(cls, parser, args):
    ct_benchmarks_util.ValidateCommandLineArgs(parser, args)

  def CreateStorySet(self, options):
    def Wait(action_runner):
      action_runner.Wait(options.wait_time)
    return page_set.CTPageSet(
      options.urls_list, options.user_agent, options.archive_data_file,
      traffic_setting=options.traffic_setting,
      cache_temperature=options.cache_temperature,
      run_page_interaction_callback=Wait)
