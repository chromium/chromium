// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

class TestHelper {
  constructor(dp) {
    this._dp = dp;
  }

  async describeNode(nodeId) {
    const response = await this._dp.DOM.resolveNode({backendNodeId: nodeId});
    return response.result && response.result.object.description ?
        `<${response.result.object.description}>` : '<invalid id>';
  }

  patchTimes(start, end, obj, fields) {
    // Add some slack to defeat time clamping.
    start -= 1;
    end += 1;
    for (const field of fields) {
      const time = obj[field] * 1000;
      if (time && (start <= time && time <= end)) {
        obj[field] = `<${typeof time}>`;
      } else if (time) {
        obj[field] = `FAIL: actual: ${time}, expected: ${start} <= time <= ${end}`
      }
    }
  }
};

return TestHelper;

})()
