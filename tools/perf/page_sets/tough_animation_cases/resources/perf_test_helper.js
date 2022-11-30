// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function() {
'use strict';

if (window.PerfTestHelper) {
  return;
}
window.PerfTestHelper = {};

var randomSeed = 3384413;
var mask = 0xffffffff;
window.PerfTestHelper.random = function() {
  var temp = randomSeed;
  // Robert Jenkins' 32 bit integer hash function.
  temp = ((temp + 0x7ed55d16) + (temp << 12)) & mask;
  temp = ((temp ^ 0xc761c23c) ^ (temp >>> 19)) & mask;
  temp = ((temp + 0x165667b1) + (temp << 5)) & mask;
  temp = ((temp + 0xd3a2646c) ^ (temp << 9)) & mask;
  temp = ((temp + 0xfd7046c5) + (temp << 3)) & mask;
  temp = ((temp ^ 0xb55a4f09) ^ (temp >>> 16)) & mask;
  randomSeed = temp;
  return (randomSeed & 0xfffffff) / 0x10000000;
};

window.PerfTestHelper.getParameter = function(parameter) {
  var match = new RegExp(parameter + '=([^&]*)').exec(window.location.search);
  if (match) {
    return match[1];
  }
  return null;
};

window.PerfTestHelper.getN = function(defaultN) {
  var match = PerfTestHelper.getParameter('N');
  if (match) {
    var n = Number(match);
    if (isNaN(n)) {
      throw 'Invalid N value: ' + match;
    }
    return n;
  }
  if (typeof defaultN === 'undefined') {
    throw 'Default N value required';
  }
  return defaultN;
};

window.PerfTestHelper.signalReady = function() {
  requestAnimationFrame(function() {
    // FIXME: We must wait at least two frames before
    // measuring to allow the GC to clean up the
    // previous test.
    requestAnimationFrame(function() {
      window.measurementReady = true;
    });
  });
};

})();
