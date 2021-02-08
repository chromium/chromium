# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Validates the rendering/frame_times metric.
"""

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
    raise Exception('Histgoram %s not found.\n%s' % (name, msg))
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

  for v2_hist_name, v3_hist_name in config['histogram_mappings'].items():
    v2_hist = GetHistogram(v2_histograms, v2_hist_name, v2_metric, 'v2')
    v3_hist = GetHistogram(v3_histograms, v3_hist_name, v3_metric, 'v3')

    utils.AssertHistogramStatsAlmostEqual(test_ctx, v2_hist, v3_hist)
    utils.AssertHistogramSamplesAlmostEqual(test_ctx, v2_hist, v3_hist)
