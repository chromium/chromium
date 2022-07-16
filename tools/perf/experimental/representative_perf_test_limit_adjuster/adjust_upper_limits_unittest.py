# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pandas as pd
import os
import sys
import unittest

from experimental.representative_perf_test_limit_adjuster import (
    adjust_upper_limits)


def create_sample_dataframe(story_name, count, avg_start, avg_step, ci_start,
                            ci_step, cpu_wal_start, cpu_wall_step):
  cols = ['stories', 'avg', 'ci_095', 'cpu_wall_time_ratio', 'index']
  df = pd.DataFrame(columns=cols)
  for idx in range(count):
    avg = avg_start + idx * avg_step
    ci = ci_start + idx * ci_step
    cpu_wall = cpu_wal_start + idx * cpu_wall_step
    df = df.append(
        {
            'stories': story_name,
            'avg': avg,
            'ci_095': ci,
            'cpu_wall_time_ratio': cpu_wall,
            'index': idx
        },
        ignore_index=True)
  return df


class TestAdjustUpperLimits(unittest.TestCase):
  def test_get_percentile_values(self):
    dataframe = create_sample_dataframe('story_name', 21, 16.0, 0.5, 0.2, 0.01,
                                        0.2, 0.01)
    limits = adjust_upper_limits.GetPercentileValues(dataframe, 0.95)

    # Given values for avg: [16, 16.5, 17, ..., 25.5, 26]
    self.assertEquals(limits['story_name']['avg'], 25.5)

    # Given values for ci_095: [0.2, 0.21, 0.22, ..., 0.39, 0.4]
    self.assertEquals(limits['story_name']['ci_095'], 0.39)

    # Given values for cpu_wall_time_ratio: [0.2, 0.21, 0.22, ..., 0.39, 0.4]
    self.assertEquals(limits['story_name']['cpu_wall_time_ratio'], 0.21)
