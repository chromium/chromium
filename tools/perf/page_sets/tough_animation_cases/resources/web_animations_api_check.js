// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
if (!Element.prototype.animate) {
  Element.prototype.animate = function() {
    return {
      pause: function() {},
    };
  };
  // Generate a dummy animation when Web Animations is not present.
  // This is to ensure frames are generated so telemetry does not see this as a
  // test failure and turn the perf tree red.
  requestAnimationFrame(function() {
    document.body.style.visibility = 'hidden';
    document.documentElement.style.transition = '1s';
    requestAnimationFrame(function() {
      document.documentElement.style.background = 'red';
    });
  });
  window.measurementReady = true;
}
