# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from enum import Enum
import os
import re
import pandas as pd

from .. import chrome
from .. import command_line


async def run_benchmark(app, output_dir, *args):
    """Runs the speedometer 3.1 benchmark over ADB and returns the results.

    Args:
        app: The chrome app to run the benchmark on.
        output_dir: The output directory of the speedometer benchmark.
        *args: Additional arguments to pass to the speedometer benchmark.

    Returns:
        A Result object containing the results of the speedometer benchmark.
    Raises:
        FileExistsError: If the output directory already exists.
    """

    def get_target_browser(channel):
        match channel:
            case chrome.Channel.STABLE:
                return "chrome-stable"
            case chrome.Channel.BETA:
                return "chrome-beta"
            case chrome.Channel.DEV:
                return "chrome-dev"
            case chrome.Channel.CANARY:
                return "chrome-canary"
            case chrome.Channel.LOCAL_BUILD:
                return "chrome-app"
            case chrome.Channel.CHROMIUM:
                return "chromium"
            case _:
                raise AssertionError(
                    f"Missing target browser for channel: {channel}.")

    # Crossbench will fail if the output directory already exists.
    if os.path.exists(output_dir):
        raise FileExistsError(f"Output directory {output_dir} already exists.")

    await command_line.run("./third_party/crossbench/cb.py", "speedometer3.1",
                           "--browser",
                           f"adb:{get_target_browser(app.channel)}",
                           "--out-dir", f"{output_dir}", *args)
    return read_result(output_dir)


def read_result(output_dir, run_index=None):
    """Reads the speedometer benchmark results from the output directory and
    returns a corresponding Result object.

    Args:
        output_dir: The output directory of the speedometer benchmark.
        run_index: The index of the run to retrieve the results for. If None,
            the aggregated results for all runs will be returned.
    Returns:
        The results of the speedometer benchmark.
    Raises:
        FileNotFoundError: If the speedometer results are not found.
    """
    json_path = f"{output_dir}/speedometer_3.1.json"
    if run_index is not None:
        json_path = f"{output_dir}/runs/{run_index}/speedometer_3.1.json.nested"

    if not os.path.exists(json_path):
        raise FileNotFoundError(
            f"Speedometer results not found at {json_path}, {output_dir}")

    df = pd.read_json(json_path)
    return Result(df.iloc[:, 0]["data"],
                  df.iloc[:, 0]["info"]) if run_index is None else Result(df)


class ResultType(Enum):
    """Type of the speedometer benchmark result."""
    VALUES = "values"  # List of all values by story or by iteration
    AVERAGE = ["average", "mean"]  # Average of all the values
    MIN = "min"  # Minimum of all the values
    MAX = "max"  # Maximum of all the values
    SUM = "sum"  # Sum of all the values
    CI = "delta"  # 95% confidence interval of the values array.
    PERCENT_CI = "percentDelta"  # CI as a percentage of the average
    UNIT = "unit"  # Unit of the speedometer benchmark result
    STDDEV = "stddev"  # Standard deviation of the values
    PERCENT_STDDEV = "stddevPercent"  # STDDEV as a percentage of the average


class Result:
    """Results of the speedometer benchmark."""

    def __init__(self, data, info=None):
        self.data = data
        self.info = info
        self.num_iterations = max(
            (int(k.split('-')[1]) + 1
             for k in self.data if k.startswith("Iteration-")),
            default=0)

    def _lookup(self, key, result_type):
        """Looks up a value in the speedometer benchmark results."""
        if key not in self.data:
            raise ValueError(f"Results do not contain the key {key}.")

        # Extract the value of the result type from the Enum, if necessary.
        rt = result_type.value if isinstance(result_type,
                                             Enum) else result_type

        if isinstance(rt, list):
            # Some result types can have different keys depending on if the
            # results are aggregated or not. But there should be only one key
            # for each result type that exists in the data. Enforce this to
            # avoid ambiguity.
            matched_rt = [r for r in rt if r in self.data[key]]
            if len(matched_rt) != 1:
                raise ValueError(
                    f"{len(matched_rt)} results found for {key} and {rt}")
            rt = matched_rt[0]

        if not isinstance(rt, str):
            raise ValueError(
                f"Invalid type {type(result_type)} for speedometer benchmark "
                + "result.")

        if rt not in self.data[key]:
            raise ValueError(
                f"Result type {rt} not found for {key} in speedometer " +
                "benchmark results.")

        return self.data[key][rt]

    def score(self, result_type=ResultType.AVERAGE):
        """
        The score of the speedometer benchmark i.e. the inverse of the
        geometric mean of the test totals.

        Args:
            result_type: The type of the speedometer benchmark result. Defaults
              to Type.AVERAGE.
        Returns:
            The score of the speedometer benchmark.
        """
        return self._lookup("Score", result_type)

    def geomean(self, result_type=ResultType.AVERAGE):
        """Geometric mean of the test totals.

        Args:
            result_type: The type of the speedometer benchmark result. Defaults
              to Type.AVERAGE.
        Returns:
            The geometric mean of the test totals.
        """
        return self._lookup("Geomean", result_type)

    def _check_is_valid_story(self, story_name):
        """Checks if the story name is valid."""
        # Story names are in the format <top_story>/<sub_story>/<sync_or_async>
        # For example, "TodoMVC-JavaScript-ES5/Adding100Items/Sync"
        parts = story_name.split("/")

        if len(parts) > 3:
            return False

        top_story = parts[0]
        sub_story = parts[1] if len(parts) > 1 else None
        sync_or_async = parts[2] if len(parts) > 2 else None

        if top_story not in _STORY_MAP.keys() or \
           (sub_story and sub_story not in _STORY_MAP[top_story]) or \
           (sync_or_async and sync_or_async not in ["Sync", "Async"]):
            return False

        return True

    def story(self, story_name, result_type=ResultType.AVERAGE):
        """Lookup the result of the speedometer benchmark for a given story.

        Args:
            story_name: The name of the story.
            result_type: The type of the speedometer benchmark result. Defaults
              to Type.AVERAGE.
        Returns:
            The result of the speedometer benchmark for the given story.
        Raises:
            ValueError: If the story name is not a valid speedometer story.
        """
        if not self._check_is_valid_story(story_name):
            raise ValueError(f"{story_name} is not a valid speedometer story.")

        return self._lookup(story_name, result_type)

    def iteration(self, index, result_type=ResultType.AVERAGE):
        """Lookup the result of the speedometer benchmark for a given iteration.

        Args:
            index: The index of the iteration to slice the results by.
            result_type: The type of the speedometer benchmark result. Defaults
              to Type.AVERAGE.
        Returns:
            The result of the speedometer benchmark for the given iteration.
        Raises:
            IndexError: If the iteration index is out of range.
        """
        if index < 0 or index >= self.num_iterations:
            raise IndexError(
                f"Iteration index {index} is out of valid range [0, " +
                f"{self.num_iterations}).")

        return self._lookup(f"Iteration-{index}-Total", result_type)


_STORY_MAP = {
    'TodoMVC-JavaScript-ES5': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-JavaScript-ES6-Webpack-Complex-DOM' : \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-WebComponents': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-React-Complex-DOM': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-React-Redux': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-Backbone': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-Angular-Complex-DOM': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-Vue': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-jQuery': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-Preact-Complex-DOM': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-Svelte-Complex-DOM': \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'TodoMVC-Lit-Complex-DOM' : \
        ['Adding100Items', 'CompletingAllItems', 'DeletingAllItems'],
    'NewsSite-Next' : ['NavigateToUS', 'NavigateToWorld', 'NavigateToPolitics'],
    'NewsSite-Nuxt' : ['NavigateToUS', 'NavigateToWorld', 'NavigateToPolitics'],
    'Editor-CodeMirror': ['Long', 'Highlight'],
    'Editor-TipTap': ['Long', 'Highlight'],
    'Charts-observable-plot' : ['Stacked by 6', 'Stacked by 20', 'Dotted'],
    'Charts-chartjs' : ['Draw scatter', 'Show tooltip', 'Draw opaque scatter'],
    'React-Stockcharts-SVG' : ['Render', 'PanTheChart', 'ZoomTheChart'],
    'Perf-Dashboard' : ['Render', 'SelectingPoints', 'SelectingRange'],
}
