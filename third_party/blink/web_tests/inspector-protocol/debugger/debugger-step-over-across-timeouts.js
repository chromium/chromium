// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that debugger StepOver will stop inside next timeout handler.');
  const debuggerId = (await dp.Debugger.enable()).result.debuggerId;
  const debuggers = new Map([[debuggerId, dp.Debugger]]);
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await dp.Runtime.evaluate({
    expression: `
      function testFunction()
      {
          setTimeout(callback1, 0);
      }

      function callback1()
      {
          setTimeout(callback2, 0);
          debugger;
      }

      function callback2()
      {
          var dummy = 42; // Should pause here.
          (function FAIL_Should_Not_Pause_Here() { debugger; })();
      }

      function scheduleTestFunction()
      {
          setTimeout(testFunction, 0);
      }
      //# sourceURL=debugger-step-over-across-timeouts.js
    `,
  });

  await dp.Runtime.evaluate({expression: 'scheduleTestFunction()'});

  let {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
      await dp.Debugger.oncePaused();
  testRunner.log('Call stack:');
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      debuggerId);

  testRunner.log('Executing StepOver...');
  await dp.Debugger.stepOver();
  await dp.Debugger.oncePaused();

  testRunner.log('Executing StepOver...');
  await dp.Debugger.stepOver();
  ({params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
       await dp.Debugger.oncePaused());
  testRunner.log('Call stack:');
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      debuggerId);

  await dp.Debugger.disable();
  testRunner.completeTest();
})
