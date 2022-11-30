# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

MEASURE_FRAME_TIME_SCRIPT = '''
window.__webui_startMeasuringFrameTime = function(name) {
  if (window.__webui_onRequestAnimationFrame) {
    window.__webui_stopMeasuringFrameTime();
  }
  window.__webui_onRequestAnimationFrame = function() {
    const now = performance.now();
    if (window.__webui_lastAnimationFrameTime) {
      performance.mark(
          `${name}:${now - window.__webui_lastAnimationFrameTime}:metric_value`);
    }
    window.__webui_lastAnimationFrameTime = now;
    if (window.__webui_onRequestAnimationFrame) {
      window.__webui_lastRequestId = requestAnimationFrame(
          window.__webui_onRequestAnimationFrame);
    }
  }
  window.__webui_lastRequestId = requestAnimationFrame(
      window.__webui_onRequestAnimationFrame);
}

window.__webui_stopMeasuringFrameTime = function() {
  if (window.__webui_lastRequestId) {
    cancelAnimationFrame(window.__webui_lastRequestId);
  }
  window.__webui_lastRequestId = null;
  window.__webui_onRequestAnimationFrame = null;
  window.__webui_lastAnimationFrameTime = null;
}
'''

START_MEASURING_FRAME_TIME = '''
window.__webui_startMeasuringFrameTime('%s')
'''

STOP_MEASURING_FRAME_TIME = '''
window.__webui_stopMeasuringFrameTime()
'''

MEASURE_JS_MEMORY = '''
performance.mark(
    `%s:${performance.memory.usedJSHeapSize}:metric_value`);
'''
