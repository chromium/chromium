// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks for setInterval.');

  const debuggerId = (await dp.Debugger.enable()).result.debuggerId;
  const debuggers = new Map([[debuggerId, dp.Debugger]]);
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await dp.Runtime.evaluate({
    expression: `
      var intervalId;
      var count = 0;

      function testFunction()
      {
          intervalId = setInterval(callback, 0);
      }

      function callback()
      {
          if (count === 0) {
              debugger;
          } else if (count === 1) {
              debugger;
          } else {
              clearInterval(intervalId);
              debugger;
          }
          ++count;
      }

      function scheduleTestFunction()
      {
          setTimeout(testFunction, 0);
      }
      //# sourceURL=async-callstack-set-interval.js
    `,
    objectGroup: 'console',
  });

  const totalDebuggerStatements = 3;
  testRunner.log('Set timer for test function.');

  let pausedPromise = dp.Debugger.oncePaused();
  const evalPromise = dp.Runtime.evaluate({
    expression: 'scheduleTestFunction()',
    objectGroup: 'console',
  });

  for (let i = 0; i < totalDebuggerStatements; ++i) {
    const {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
        await pausedPromise;
    testRunner.log(`Call stack ${i + 1}:`);
    await testRunner.logStackTrace(
        debuggers,
        {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
        debuggerId);
    if (i + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
    } else {
      dp.Debugger.onPaused(async () => {
        testRunner.log('Unexpected pause after clearInterval.');
        await dp.Debugger.resume({terminateOnResume: false});
      });
    }
    await dp.Debugger.resume({terminateOnResume: false});
  }

  await evalPromise;

  const countAfterClear = await dp.Runtime.evaluate({
    expression: 'new Promise(resolve => setTimeout(() => resolve(count), 0))',
    awaitPromise: true,
    returnByValue: true,
    objectGroup: 'console',
  });
  testRunner.log(`Final count: ${countAfterClear.result.result.value}`);

  testRunner.completeTest();
});
