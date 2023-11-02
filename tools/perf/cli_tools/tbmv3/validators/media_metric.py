# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Validates the media_metric.
"""

from cli_tools.tbmv3.validators import simple_validator


def CompareHistograms(test_ctx):
  v2_metric = 'mediaMetric'
  v3_metric = 'media_metric'
  v2_histograms = test_ctx.RunTBMv2(v2_metric)
  v3_histograms = test_ctx.RunTBMv3(v3_metric)

  simple_config = {
      'v2_metric': v2_metric,
      'v3_metric': v3_metric,
      # 1 microsecond precision - default for ms unit histograms.
      'float_precision': 1e-3,
      'histogram_mappings': {
          # mappings are 'v2_histogram: 'v3_histogram'.
          'time_to_video_play': 'media::time_to_video_play',
          'time_to_audio_play': 'media::time_to_audio_play',
          # Dropped frame count is broken in the TBMv2 metric.
          # 'dropped_frame_count': 'media::dropped_frame_count',
          'buffering_time': 'media::buffering_time',
          # Roughness is reported as double in the v3 metric, but as int in v2.
          'roughness': ('media::roughness', 1),
          'freezing': 'media::freezing'
      },
  }

  simple_validator.CompareSimpleHistograms(test_ctx, simple_config,
                                           v2_histograms, v3_histograms)

  # seek time histograms are merged.
  seek_time_histograms = [
      # v3 histogram => set of v2 histograms that are merged into it.
      ['media::seek_time', ['seek_time_0_5', 'seek_time_9']],
      [
          'media::pipeline_seek_time',
          ['pipeline_seek_time_0_5', 'pipeline_seek_time_9']
      ],
  ]

  for entry in seek_time_histograms:
    v3_hist_name = entry[0]
    v3_hist = simple_validator.OptionalGetHistogram(v3_histograms, v3_hist_name,
                                                    v3_metric, 'v3')

    v2_hists = []
    for v2_hist_name in entry[1]:
      v2_hist = simple_validator.OptionalGetHistogram(v2_histograms,
                                                      v2_hist_name, v2_metric,
                                                      'v2')
      if v2_hist is None:
        continue
      v2_hists += [v2_hist]

    if v3_hist is None:
      if len(v2_hists) > 0:
        raise Exception('Expected a %s v3 histogram, but none exists' %
                        (v3_hist_name))
      continue

    if len(v2_hists) == 0:
      raise Exception('Have a %s v3 histogram but no matching v2 ones' %
                      (v3_hist_name))

    v2_samples = []
    for v2_hist in v2_hists:
      v2_samples += [s for s in v2_hist.sample_values if s is not None]
    v3_samples = [s for s in v3_hist.sample_values if s is not None]

    test_ctx.assertEqual(len(v2_samples), len(v3_samples))
    v2_samples.sort()
    v3_samples.sort()
    for v2_sample, v3_sample in zip(v2_samples, v3_samples):
      test_ctx.assertAlmostEqual(v2_sample,
                                 v3_sample,
                                 delta=simple_config['float_precision'])
