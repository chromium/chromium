// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Test trace events for scheduler.postTask()');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;

  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing('devtools.timeline');
  await session.evaluateAsync(`
(function() {

          function task() {
            controller.abort();
          }

          const controller = new TaskController({priority: 'background'});
          const signal = controller.signal;

          const p1 = scheduler.postTask(task);
          const p2 = scheduler.postTask(() => {}, {signal});
          p2.catch(() => {});

          return new Promise(resolve => {
            setTimeout(() => resolve(p1), 500)
          });
})()
  `);
  const events = await tracingHelper.stopTracing(/devtools\.timeline/);

  const scheduleEvents = tracingHelper.findEvents('SchedulePostTaskCallback', Phase.INSTANT);
  testRunner.log(`Number of SchedulePostTaskCallback events found: ${scheduleEvents.length}`);
  tracingHelper.logEventShape(scheduleEvents[0]);
  testRunner.log(`Schedule event 1 priority: ${scheduleEvents[0].args.data.priority}`);
  tracingHelper.logEventShape(scheduleEvents[1]);
  testRunner.log(`Schedule event 2 priority: ${scheduleEvents[1].args.data.priority}`);

  testRunner.log('Found RunPostTaskCallback event');
  const runEvent = tracingHelper.findEvent('RunPostTaskCallback', Phase.COMPLETE);
  tracingHelper.logEventShape(runEvent);
  testRunner.log(`RunPostTaskCallback priority: ${runEvent.args.data.priority}`);
  testRunner.log('Found AbortPostTaskCallback event');
  const abortEvent = tracingHelper.findEvent('AbortPostTaskCallback', Phase.COMPLETE);
  tracingHelper.logEventShape(abortEvent);
  testRunner.completeTest();
});
