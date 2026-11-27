// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests event listener breakpoints for WebAudio.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Debugger.setBreakpointsActive({active: true});

  await dp.Runtime.evaluate({
    expression: 'let audioContext;',
    objectGroup: 'console',
  });

  const steps = [
    {
      eventName: 'audioContextCreated',
      expression: 'audioContext = new AudioContext()',
      message: 'Successfully paused after AudioContext construction.',
      awaitPromise: false,
    },
    {
      eventName: 'audioContextSuspended',
      expression: 'audioContext.suspend()',
      message: 'Successfully paused after AudioContext suspension.',
      awaitPromise: true,
    },
    {
      eventName: 'audioContextResumed',
      expression: 'audioContext.resume()',
      message: 'Successfully paused after AudioContext resumption.',
      awaitPromise: true,
    },
    {
      eventName: 'audioContextClosed',
      expression: 'audioContext.close()',
      message: 'Successfully paused after AudioContext closure.',
      awaitPromise: true,
    },
  ];

  for (const {eventName, expression, message, awaitPromise} of steps) {
    await dp.EventBreakpoints.setInstrumentationBreakpoint({eventName});
    const pausedPromise = dp.Debugger.oncePaused();
    const evalPromise = dp.Runtime.evaluate({
      expression,
      objectGroup: 'console',
      awaitPromise,
    });
    const {params: {reason, data}} = await pausedPromise;
    testRunner.log('Script execution paused.');
    testRunner.log({data, reason});
    testRunner.log(message);
    const resumedPromise = dp.Debugger.onceResumed();
    await dp.Debugger.resume({terminateOnResume: false});
    await resumedPromise;
    const evalResult = await evalPromise;
    if (evalResult.error || evalResult.result?.exceptionDetails) {
      testRunner.log('FAIL: ' + JSON.stringify(evalResult));
    }
    testRunner.log('Script execution resumed.');
  }

  testRunner.completeTest();
});
