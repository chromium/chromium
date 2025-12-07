# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import asyncio
import os
import tempfile
from contextlib import ExitStack

from .. import trace


async def measure_cold_start(app, url, trace_file=None, trace_config=None):
    """Measures Chrome's cold start time by checking the value recorded into
    `Startup.Android.Cold.TimeToFirstVisibleContent4`

    See `tools/metrics/histograms/metadata/startup/histograms.xml` for more
    details about when the metric is recorded.

    This function ensures the app is stopped, performs a cold start, while
    recording a trace and then queries the trace for the histogram value.

    Args:
        app: The chrome.App object with which to start and stop chrome
        url: The URL to launch the Chrome with.
        trace_file: An optional `trace.TraceFile` object to store the trace.
                    If not provided, a temporary file will be created.
        trace_config: An optional path to a Perfetto trace config file.
                      If not provided, a default config is used.

    Returns:
        The cold startup time in milliseconds.
    """
    if trace_config is None:
        # This assumes the current working directory is the chromium src root.
        trace_config = os.path.join(os.getcwd(), "tools", "android",
                                    "colabutils", "res",
                                    "cold_start_trace_cfg.pbtxt")

    if not os.path.exists(trace_config):
        raise FileNotFoundError(f"Trace config not found at {trace_config}")

    with ExitStack() as stack:
        # If no trace file is provided, create a temporary one that will be
        # cleaned up upon exiting the context.
        if trace_file is None:
            temporary_recorded_trace = stack.enter_context(
                tempfile.NamedTemporaryFile(mode='w'))
            trace_file = trace.TraceFile(temporary_recorded_trace.name)

        return await _measure_cold_start(app, url, trace_file, trace_config)


async def _measure_cold_start(app, url, trace_file, trace_config):
    return await _measure_startup(
        app, url, trace_file, trace_config, _STARTUP_TIME_QUERY,
        "Could not find histogram sample for "
        "Startup.Android.Cold.TimeToFirstVisibleContent4 in the trace.")


async def _measure_first_frame(app, url, trace_file, trace_config):
    return await _measure_startup(
        app, url, trace_file, trace_config, _FIRST_FRAME_TIME_QUERY,
        "Could not find logcat message for "
        "'Displayed com.google.android.apps.chrome/"
        "org.chromium.chrome.browser.ChromeTabbedActivity' in the trace.")


async def _measure_startup(app, url, trace_file, trace_config, query,
                           error_message):
    # Stop the app before recording the trace so that the trace cleanly shows
    # the cold start
    await app.stop()

    async with trace_file.record(trace_config):
        # Allow some time for Perfetto to start recording the trace before the
        # app starts.
        await asyncio.sleep(2)

        await app.start(url=url)
        await asyncio.sleep(5)  # Startup should not take more than 5 seconds
        await app.stop()

    df = await trace_file.query(query)
    try:
        return int(df.iloc[0, 0])
    except IndexError:
        raise LookupError(error_message)



_STARTUP_TIME_QUERY = r"""
INCLUDE PERFETTO MODULE viz.slices;

SELECT
-- Select the display_value from the second join to the args table.
-- This will be the value for 'chrome_histogram_sample.sample'.
args_sample.display_value AS sample_value
FROM
_viz_slices_for_ui_table AS slice
-- First join to args table to FIND the event by its name.
LEFT JOIN
args AS args_name ON slice.arg_set_id = args_name.arg_set_id
-- Second join to the same args table to GET the sample value from that event.
LEFT JOIN
args AS args_sample ON slice.arg_set_id = args_sample.arg_set_id
WHERE
-- Use the first join to filter for the specific event name.
args_name.display_value = 'Startup.Android.Cold.TimeToFirstVisibleContent4'
AND args_name.key = 'chrome_histogram_sample.name'
-- Use the second join to specify which key's value you want to select.
AND args_sample.key = 'chrome_histogram_sample.sample'
"""


async def measure_first_frame(app, url, trace_file=None, trace_config=None):
    """Measures Chrome's time to first frame.

    This function ensures the app is stopped, performs a cold start, while
    recording a trace and then queries the trace for the first frame time.
    The display time is reported by android in logcat.

    Args:
        app: The chrome.App object with which to start and stop chrome
        url: The URL to launch the Chrome with.
        trace_file: An optional `trace.TraceFile` object to store the trace.
                    If not provided, a temporary file will be created.
        trace_config: An optional path to a Perfetto trace config file.
                      If not provided, a default config is used.

    Returns:
        The time to first frame in milliseconds.
    """
    if trace_config is None:
        # This assumes the current working directory is the chromium src root.
        trace_config = os.path.join(
            os.getcwd(), "tools", "android", "colabutils", "res",
            "cold_start_trace_with_logcat_timing_cfg.pbtxt")

    if not os.path.exists(trace_config):
        raise FileNotFoundError(f"Trace config not found at {trace_config}")

    with ExitStack() as stack:
        # If no trace file is provided, create a temporary one that will be
        # cleaned up upon exiting the context.
        if trace_file is None:
            temporary_recorded_trace = stack.enter_context(
                tempfile.NamedTemporaryFile(mode='w'))
            trace_file = trace.TraceFile(temporary_recorded_trace.name)

        return await _measure_first_frame(app, url, trace_file, trace_config)


_FIRST_FRAME_TIME_QUERY = r"""
SELECT
  CAST(
    REGEXP_EXTRACT(
      msg,
      'Displayed com.google.android.apps.chrome/org.chromium.chrome.browser.ChromeTabbedActivity[^+]+\+(.+)ms'
    ) AS INT
  ) AS launch_time
FROM
  android_logs
WHERE
  msg LIKE 'Displayed %'
"""
