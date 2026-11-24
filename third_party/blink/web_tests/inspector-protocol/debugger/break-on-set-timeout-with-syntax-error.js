// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that scheduled pause is cleared after processing event with empty handler.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  await session.evaluate(`
    function runSetTimeoutWithSyntaxError() {
      setTimeout({}, 0);
    }

    function executeSomeCode() {
      debugger; // should stop here not earlier
    }
  `);

  const pausedEvents = [];
  dp.Debugger.onPaused(event => pausedEvents.push(event));

  await dp.EventBreakpoints.setInstrumentationBreakpoint({
    eventName: 'setTimeout.callback',
  });

  const scriptFailedPromise = dp.Debugger.onceScriptFailedToParse();
  const exceptionPromise = dp.Runtime.onceExceptionThrown();
  await session.evaluate('runSetTimeoutWithSyntaxError()');
  const [, exceptionEvent] = await Promise.all([
    scriptFailedPromise,
    exceptionPromise,
  ]);
  testRunner.log(`Exception thrown: ${
      exceptionEvent.params.exceptionDetails.exception.className}`);
  testRunner.log(`Paused count before executeSomeCode: ${pausedEvents.length}`);

  const pausedPromise = dp.Debugger.oncePaused();
  const evalPromise = session.evaluate('executeSomeCode()');
  const {params: {callFrames, reason, data}} = await pausedPromise;
  testRunner.log(`Paused reason: ${reason}`);
  if (data) {
    testRunner.log(`Paused data: ${JSON.stringify(data)}`);
  }
  testRunner.logCallFrames(callFrames);

  await dp.EventBreakpoints.removeInstrumentationBreakpoint({
    eventName: 'setTimeout.callback',
  });
  await dp.Debugger.resume();
  await evalPromise;
  testRunner.log(`Total paused count: ${pausedEvents.length}`);

  testRunner.completeTest();
});
