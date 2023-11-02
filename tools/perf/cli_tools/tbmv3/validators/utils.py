# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def AssertHistogramStatsAlmostEqual(test_ctx, v2_hist, v3_hist, precision=1e-3):
  """Asserts major histogram statistics are close enough.

  sum, mean, max, min are asserted to be within 3 decimal places.
  count is asserted to be exactly equal."""
  v2_running = v2_hist.running
  v3_running = v3_hist.running
  test_ctx.assertAlmostEqual(v2_running.mean, v3_running.mean, delta=precision)
  test_ctx.assertAlmostEqual(v2_running.sum, v3_running.sum, delta=precision)
  test_ctx.assertAlmostEqual(v2_running.max, v3_running.max, delta=precision)
  test_ctx.assertAlmostEqual(v2_running.min, v3_running.min, delta=precision)
  test_ctx.assertEqual(v2_running.count, v3_running.count)


def AssertHistogramSamplesAlmostEqual(test_ctx,
                                      v2_hist,
                                      v3_hist,
                                      precision=1e-3):
  v2_samples = [s for s in v2_hist.sample_values if s is not None]
  v3_samples = [s for s in v3_hist.sample_values if s is not None]

  test_ctx.assertEqual(len(v2_samples), len(v3_samples))
  v2_samples.sort()
  v3_samples.sort()
  for v2_sample, v3_sample in zip(v2_samples, v3_samples):
    test_ctx.assertAlmostEqual(v2_sample, v3_sample, delta=precision)
