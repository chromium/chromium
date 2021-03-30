# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Set the required tracing categories specified by page_cycler_v2
def AugmentOptionsForLoadingMetrics(tbm_options):
  cat_filter = tbm_options.config.chrome_trace_config.category_filter

  # "blink.console" is used for marking ranges in
  # cache_temperature.MarkTelemetryInternal.
  cat_filter.AddIncludedCategory('blink.console')

  # "navigation" and "blink.user_timing" are needed to capture core
  # navigation events.
  cat_filter.AddIncludedCategory('navigation')
  cat_filter.AddIncludedCategory('blink.user_timing')

  # "loading" is needed for first-meaningful-paint computation.
  cat_filter.AddIncludedCategory('loading')

  # "toplevel" category is used to capture TaskQueueManager events
  # necessary to compute time-to-interactive.
  cat_filter.AddIncludedCategory('toplevel')

  # "network" category is used to capture ResourceLoad events necessary to
  # properly compute time-to-interactive.
  cat_filter.AddDisabledByDefault('disabled-by-default-network')

  tbm_options.AddTimelineBasedMetric('loadingMetric')
  return tbm_options
