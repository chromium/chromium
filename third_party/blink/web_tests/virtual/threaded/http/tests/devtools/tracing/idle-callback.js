// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Timeline events emitted when idle callback is scheduled and fired.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.evaluateInPagePromise(`
      function performActions(idleWarningAddOn)
      {
          var callback;
          var promise = new Promise((fulfill) => callback = fulfill);
          var requestId = window.requestIdleCallback(idleCallback);
          window.cancelIdleCallback(requestId);
          window.requestIdleCallback(idleCallback);
          function idleCallback()
          {
              window.requestIdleCallback(slowIdleCallback);
          }
          function slowIdleCallback(deadline)
          {
              while (deadline.timeRemaining()) {};
              var addOnDeadline = performance.now() + idleWarningAddOn;
              while (performance.now() < addOnDeadline) {};
              if (callback)
                  callback();
          }
          return promise;
      }
  `);

  PerformanceTestRunner.invokeAsyncWithTimeline(
      `(() => performActions(${TimelineModel.TimelineModel.Thresholds.IdleCallbackAddon}))`, finish);

  function finish() {
    PerformanceTestRunner.printTimelineRecordsWithDetails('RequestIdleCallback');
    PerformanceTestRunner.printTimelineRecordsWithDetails('CancelIdleCallback');
    PerformanceTestRunner.printTimelineRecordsWithDetails('FireIdleCallback');
    TestRunner.completeTest();
  }
})();
