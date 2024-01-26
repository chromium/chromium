// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the data of Garbage Collection trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  const Phase = TracingHelper.Phase;
  await tracingHelper.startTracing('devtools.timeline,v8');
  await session.evaluateAsync(`
        GCController.minorCollect();
        GCController.collectAll();
    `);
  await tracingHelper.stopTracing(/devtools\.timeline|v8/);
  const minorGC = tracingHelper.findEvent('MinorGC', Phase.COMPLETE);
  const majorGC = tracingHelper.findEvent('MajorGC', Phase.COMPLETE);

  testRunner.log('Got a MajorGC event');
  tracingHelper.logEventShape(majorGC);
  testRunner.log('Got a MinorGC event');
  tracingHelper.logEventShape(minorGC);
  testRunner.completeTest();
})
