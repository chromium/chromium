// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
window.testRunner = {};
window.testRunner.isDone = false;

testRunner.waitUntilDone = function() {};
testRunner.dumpAsText = function() {};
testRunner.notifyDone = function() {
  this.isDone = true;
};

testRunner.telemetryIsRunning = true;

// If this is true, blink_perf tests are paused waiting for telemetry to set up
// measurements (tracing, profiling, etc.). After this setup, the test driver
// should invoke |scheduleTestRun| to start the actual test procedure.
testRunner.isWaitingForTelemetry = false;

testRunner.waitForTelemetry = function(tracingCategories, scheduleTestRun) {
  this.tracingCategories = tracingCategories;
  this.scheduleTestRun = scheduleTestRun;
  this.isWaitingForTelemetry = true;
}

testRunner.stopTracingAndMeasure = function(traceEventsToMeasure, callback) {
  testRunner.traceEventsToMeasure = traceEventsToMeasure;
  callback();
}

window.GCController = {};

GCController.collect = function() {
  gc();
};
GCController.collectAll = function() {
  for (var i = 0; i < 7; ++i)
    gc();
};
})();
