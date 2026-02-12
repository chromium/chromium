# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import pandas as pd
import io
import contextlib
import asyncio
import signal
from jinja2 import Template
from contextlib import ExitStack
import tempfile

from . import command_line


class TraceFile:
    """A class to represent and query a recorded trace file."""

    def __init__(self, trace_file):
        """Initializes the TraceFile object with the path to the trace file.

        Args:
            trace_file: The path to the recorded trace file.
        """
        self.trace_file = trace_file

    async def query(self, query):
        """Queries the trace file using the trace_processor tool.

        Args:
            query: The SQL query string to execute on the trace.

        Returns:
            The result of the query as a pandas dataframe.
        """
        result = await command_line.run(
            "third_party/perfetto/tools/trace_processor", self.trace_file,
            "-Q", query)
        # Use io.StringIO to treat the string as a file and read it with pandas.
        return pd.read_csv(io.StringIO(result.stdout), na_values=['[NULL]'])

    @contextlib.asynccontextmanager
    async def record(self, config_file):
        """Records a trace from the configured device using the specified
        config.

        Args:
            config_file: The config file to use for recording.
        """
        recording_task = asyncio.create_task(
            command_line.run("third_party/perfetto/tools/record_android_trace",
                             "-o",
                             self.trace_file,
                             "-c",
                             config_file,
                             "-n",
                             interruption_signal=signal.SIGINT))
        if recording_task.done():
            Exception(f"Recording failed: {recording_task.result()}")

        try:
            yield
        finally:
            recording_task.cancel()
            try:
                await recording_task
            except asyncio.CancelledError:
                pass


@contextlib.contextmanager
def histograms_trace_config(histograms, duration=10000):
    """
    Create a temporary trace configuration file for recording the histogram(s)
    listed in `histograms`.

    Args:
        histograms: A string or a list of strings with the name of the
                    histogram(s) to record.
        duration: The duration of the trace.

    Returns:
        The path to the temporary trace configuration file.
    """
    if isinstance(histograms, str):
        histograms = [histograms]
    elif not isinstance(histograms, list):
        raise TypeError("histograms must be a string or a list of strings")

    # This assumes the current working directory is the chromium src root.
    jinja_file = os.path.join(os.getcwd(), "tools", "android", "colabutils",
                              "res", "histogram_trace_cfg.pbtxt.j2")
    with open(jinja_file, 'r') as f:
        template_content = f.read()
    template = Template(template_content)
    rendered_config = template.render(histogram_names=histograms,
                                      duration=duration)

    with tempfile.NamedTemporaryFile(mode='w') as temporary_config_file:
        temporary_config_file.write(rendered_config)
        temporary_config_file.flush()
        yield temporary_config_file.name


def histogram_values_query(histogram):
    """
    Create a perfetto trace query to obtain all recorded values of a histogram.

    Args:
        histogram: The name of the histogram to query.

    Returns:
        A string containing the query.
    """
    jinja_file = os.path.join(os.getcwd(), "tools", "android", "colabutils",
                              "res", "histogram_query.j2")
    with open(jinja_file, 'r') as f:
        template_content = f.read()
    template = Template(template_content)
    rendered_config = template.render(histogram_name=histogram)
    return rendered_config
