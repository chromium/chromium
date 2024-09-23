# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from devil import base_error as devil_error  # pylint: disable=import-error
from devil.android import device_utils  # pylint: disable=import-error
from telemetry.web_perf import timeline_based_measurement
from telemetry.timeline import chrome_trace_config

_JS_FLAGS_SWITCH = '--js-flags='
LOW_END_DEVICE_MEMORY_KB = 1024 * 1024  # 1 GB


def GetDeviceTotalMemory():
  try:
    devices = device_utils.DeviceUtils.HealthyDevices()
    if not devices:
      return None
    mem_info = devices[0].ReadFile('/proc/meminfo')
  except devil_error.BaseError:
    return None
  for line in mem_info.splitlines():
    if line.startswith('MemTotal:'):
      return int(line.split()[1])
  return None


def AppendJSFlags(options, js_flags):
  existing_js_flags = ''
  # There should be only one occurence of --js-flags in the browser flags. When
  # there are multiple flags, only one of them would be used. Append any
  # additional js_flags to the existing flags (if present).
  for extra_arg in options.extra_browser_args:
    if extra_arg.startswith(_JS_FLAGS_SWITCH):
      # Find and remove the set of existing js_flags.
      existing_js_flags = extra_arg[len(_JS_FLAGS_SWITCH):]
      options.RemoveExtraBrowserArg(extra_arg)
      break

  options.AppendExtraBrowserArgs([
      # Add a new --js-flags which includes previous flags.
      '%s%s %s' %  (_JS_FLAGS_SWITCH, js_flags, existing_js_flags)
  ])


def AugmentOptionsForV8Metrics(options, enable_runtime_call_stats=True):
  categories = [
      # Disable all categories by default.
      '-*',
      # Memory categories.
      'disabled-by-default-memory-infra',
      'toplevel',
      # V8 categories.
      'cppgc',
      'disabled-by-default-cppgc',
      'disabled-by-default-v8.gc',
      'v8',
      'v8.wasm',
      'v8.console',
      'webkit.console',
      # Blink categories.
      'blink.resource',
      'partition_alloc',
      # Needed for the metric reported by page.
      'blink.user_timing'
  ]

  options.ExtendTraceCategoryFilter(categories)
  if enable_runtime_call_stats:
    options.AddTraceCategoryFilter('disabled-by-default-v8.runtime_stats')

  options.config.enable_android_graphics_memtrack = True
  # Trigger periodic light memory dumps every 1000 ms.
  memory_dump_config = chrome_trace_config.MemoryDumpConfig()
  memory_dump_config.AddTrigger('light', 1000)
  options.config.chrome_trace_config.SetMemoryDumpConfig(memory_dump_config)

  # On low-end devices there's not enough memory to hold 400Mb buffer (See
  # crbug.com/1218139).
  device_memory = GetDeviceTotalMemory()
  if device_memory and device_memory < LOW_END_DEVICE_MEMORY_KB:
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(200 * 1024)
  else:
    options.config.chrome_trace_config.SetTraceBufferSizeInKb(400 * 1024)

  metrics = [
      'blinkResourceMetric',
      'consoleErrorMetric',
      'expectedQueueingTimeMetric',
      'gcMetric',
      'memoryMetric',
      'reportedByPageMetric',
      'wasmMetric',
  ]
  options.ExtendTimelineBasedMetric(metrics)
  if enable_runtime_call_stats:
    options.AddTimelineBasedMetric('runtimeStatsTotalMetric')
  return options


class V8PerfMixin(object):
  """Base class for V8 benchmarks that measure RuntimeStats,
  eqt, gc and memory metrics.
  """

  def CreateCoreTimelineBasedMeasurementOptions(self):
    options = timeline_based_measurement.Options()
    AugmentOptionsForV8Metrics(options)
    return options

  def SetExtraBrowserOptions(self, options):
    options.AppendExtraBrowserArgs(
        '--enable-blink-features=BlinkRuntimeCallStats')
    options.AppendExtraBrowserArgs(['--disable-popup-blocking'])
