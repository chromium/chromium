# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def AssertHistogramStatsAlmostEqual(test_ctx, v2_hist, v3_hist):
  """Asserts major histogram statistics are close enough.

  sum, mean, max, min are asserted to be within 3 decimal places.
  count is asserted to be exactly equal."""
  v2_running = v2_hist.running
  v3_running = v3_hist.running
  test_ctx.assertAlmostEqual(v2_running.mean, v3_running.mean, 3)
  test_ctx.assertAlmostEqual(v2_running.sum, v3_running.sum, 3)
  test_ctx.assertAlmostEqual(v2_running.max, v3_running.max, 3)
  test_ctx.assertAlmostEqual(v2_running.min, v3_running.min, 3)
  test_ctx.assertEqual(v2_running.count, v3_running.count)
