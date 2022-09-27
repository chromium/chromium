// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

class TestHelper {
  constructor(dp) {
    this._dp = dp;
    this._startTicks = performance.now();
  }

  async describeNode(nodeId) {
    const response = await this._dp.DOM.resolveNode({backendNodeId: nodeId});
    return response.result && response.result.object.description ?
        `<${response.result.object.description}>` : '<invalid id>';
  }

  async patchTimes(obj, fields) {
    const startTime = (await this._dp.Runtime.evaluate({
        expression: 'window.performance.timeOrigin',
        returnByValue: true})).result.result.value;
    const endTicks = performance.now();
    // Ensure we're using monotonic time within the test duration.
    const endTime = startTime + (endTicks - this._startTicks);
    for (const field of fields) {
      const time = obj[field] * 1000;
      if (time && (startTime <= time && time <= endTime)) {
        obj[field] = `<${typeof time}>`;
      } else if (time) {
        obj[field] = `FAIL: actual: ${time}, expected: ${startTime} <= time <= ${endTime}`;
      }
    }
  }
};

return TestHelper;

})()
