# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Validates the rendering/frame_times metric.
"""

import sys
from cli_tools.tbmv3.validators import utils

CONFIG_FORMAT = ('Config must contain "v2_metric", "v3_metric", and '
                 '"histogram_mappings."')


def CheckConfig(simple_config):
  name = simple_config.name
  config = simple_config.config

  if 'v2_metric' not in config:
    raise Exception('config %s missing "v2_metric"\n%s' % (name, CONFIG_FORMAT))
  if 'v3_metric' not in config:
    raise Exception('config %s missing "v3_metric"\n%s' % (name, CONFIG_FORMAT))
  if 'histogram_mappings' not in config:
    raise Exception('config %s missing "histogram_mappings"\n%s' %
                    (name, CONFIG_FORMAT))


def GetHistogram(histogram_set, name, metric, version):
  hists = histogram_set.GetHistogramsNamed(name)
  if len(hists) == 0:
    msg = ('List of histograms produced by TBM%s metric %s:\n' %
           (version, metric))
    msg += '\n'.join([h.name for h in histogram_set])
    raise Exception('Histogram %s not found.\n%s' % (name, msg))
  if len(hists) > 1:
    raise Exception('Multiple histograms named %s found for TBM%s metric %s' %
                    (name, version, metric))
  return hists[0]


def CompareHistograms(test_ctx):
  CheckConfig(test_ctx.simple_config)
  config = test_ctx.simple_config.config

  v2_metric = config['v2_metric']
  v3_metric = config['v3_metric']
  v2_histograms = test_ctx.RunTBMv2(v2_metric)
  v3_histograms = test_ctx.RunTBMv3(v3_metric)

  metric_precision = config['float_precision']

  for v2_hist_name, v3_hist_info in config['histogram_mappings'].items():
    if isinstance(v3_hist_info, str):
      v3_hist_name = v3_hist_info
      precision = metric_precision
    elif isinstance(v3_hist_info, tuple):
      v3_hist_name = v3_hist_info[0]
      precision = v3_hist_info[1]
    else:
      raise Exception('v3_histogram must be either string of v3_histogram '
                      ' name of (v3_hist_name, precision) tuple.')

    v2_hist = GetHistogram(v2_histograms, v2_hist_name, v2_metric, 'v2')
    v3_hist = GetHistogram(v3_histograms, v3_hist_name, v3_metric, 'v3')

    try:
      utils.AssertHistogramStatsAlmostEqual(test_ctx, v2_hist, v3_hist,
                                            precision)
      utils.AssertHistogramSamplesAlmostEqual(test_ctx, v2_hist, v3_hist,
                                              precision)
    except AssertionError as err:
      message = (
          'Error comparing TBMv2 histogram %s with TBMv3 histogram %s: %s' %
          (v2_hist.name, v3_hist.name, err.message))
      raise AssertionError, message, sys.exc_info()[2]
