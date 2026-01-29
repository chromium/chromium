# Copyright 2025 Google LLC.
# Copyright (c) Microsoft Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import os
import statistics
import time
from pathlib import Path

import pytest
from test_helpers import execute_command, goto_url

ITERATIONS = int(os.environ.get('ITERATIONS', 10))


def log_metric(test_name, name, value, unit='ms'):
    os_name = os.environ.get('OS', 'unknownOs')
    head = os.environ.get('HEAD', 'unknownHead')
    runner = os.environ.get('RUNNER', 'unknownRunner')
    metrics_json_file = os.environ.get('METRICS_JSON_FILE')
    metric = {
        'name': f'{os_name}-{head}-{runner}:{test_name}_{name}',
        'value': value,
        'unit': unit,
        'extra': f'{os_name}-{head}:e2e-perf-metric'
    }
    if metrics_json_file:
        with open(metrics_json_file, 'a') as f:
            f.write(json.dumps(metric) + ',\n')
    else:
        print(f"PERF_METRIC:{json.dumps(metric)}")


async def capture_screenshot(websocket, context_id):
    await execute_command(
        websocket,
        {
            "method": "browsingContext.captureScreenshot",
            "params": {
                "origin": "document",
                "context": context_id
            }
        },
        # Increase timeout for screenshots.
        timeout=60)


# Timeout 10 minutes.
@pytest.mark.timeout(10 * 60)
@pytest.mark.asyncio
async def test_performance_screenshot(websocket, context_id,
                                      current_test_name):
    await execute_command(
        websocket, {
            "method": "browsingContext.setViewport",
            "params": {
                "context": context_id,
                "viewport": {
                    "width": 800,
                    "height": 600,
                },
                "devicePixelRatio": None
            }
        })

    await goto_url(
        websocket, context_id,
        f'file://{Path(__file__).parent.resolve()}/resources/long_page.html')

    # Pre-warm.
    await capture_screenshot(websocket, context_id)

    samples = []
    for i in range(ITERATIONS):
        start_time = time.perf_counter()
        await capture_screenshot(websocket, context_id)
        samples.append(time.perf_counter() - start_time)

    mean_value = statistics.mean(samples) * 1000
    median_value = statistics.median(samples) * 1000
    p10_value = sorted(samples)[int(len(samples) * 0.1)] * 1000

    log_metric(current_test_name, 'mean', mean_value)
    log_metric(current_test_name, 'median', median_value)
    log_metric(current_test_name, 'p10', p10_value)
