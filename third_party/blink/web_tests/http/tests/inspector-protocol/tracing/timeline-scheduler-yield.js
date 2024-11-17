// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Test trace events for scheduler.yield()');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;

  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing('devtools.timeline');
  await session.evaluateAsync(`
      (function() {
          const p1 = scheduler.postTask(async () => {
            await scheduler.yield();
          }, {priority: 'user-blocking'});

          const taskController = new TaskController({priority: 'background'});
          const p2 = scheduler.postTask(async () => {
            // Cancel in higher-priority task that will run before continuation.
            scheduler.postTask(() => taskController.abort());

            await scheduler.yield();
          }, {signal: taskController.signal});

          return Promise.allSettled([p1, p2]);
      })()
  `);
  const events = await tracingHelper.stopTracing(/devtools\.timeline/);

  const scheduleEvents = tracingHelper.findEvents('ScheduleYieldContinuation', Phase.INSTANT);
  testRunner.log(`Number of ScheduleYieldContinuation events found: ${scheduleEvents.length}`);

  testRunner.log(`Schedule event 1 priority: ${scheduleEvents[0].args.data.priority}`);
  tracingHelper.logEventShape(scheduleEvents[0]);

  testRunner.log(`Schedule event 2 priority: ${scheduleEvents[1].args.data.priority}`);
  tracingHelper.logEventShape(scheduleEvents[1]);

  const runEvents = tracingHelper.findEvents('RunYieldContinuation', Phase.COMPLETE);
  testRunner.log(`Number of RunYieldContinuation events found: ${runEvents.length}`);
  testRunner.log(`RunYieldContinuation priority: ${runEvents[0].args.data.priority}`);
  tracingHelper.logEventShape(runEvents[0]);

  const abortEvents = tracingHelper.findEvents('AbortYieldContinuation', Phase.COMPLETE);
  testRunner.log(`Number of AbortYieldContinuation events found: ${abortEvents.length}`);
  tracingHelper.logEventShape(abortEvents[0]);

  const schedulerIdsDifferent = scheduleEvents[0].args.data.taskId !== scheduleEvents[1].args.data.taskId;
  testRunner.log(`Schedule event ids are different: ${schedulerIdsDifferent}`);

  const firstIdsMatch = scheduleEvents[0].args.data.taskId === runEvents[0].args.data.taskId;
  testRunner.log(`First schedule and run event have same id: ${firstIdsMatch}`);
  const secondIdsMatch = scheduleEvents[1].args.data.taskId === abortEvents[0].args.data.taskId;
  testRunner.log(`Second schedule and abort event have same id: ${secondIdsMatch}`);

  testRunner.completeTest();
});
