// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that debugger StepInto will stop inside next timeout handler.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();
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
      //# sourceURL=debugger-step-into-across-timeouts.js`,
    objectGroup: 'console',
  });

  function logStackTrace({callFrames, asyncStackTrace}) {
    testRunner.log('Call stack:');
    callFrames.forEach((frame, index) => {
      const url = scripts.get(frame.location.scriptId) || frame.url;
      testRunner.log(`    ${index}) ${frame.functionName} (${url}:${
          frame.location.lineNumber + 1})`);
    });
    for (let trace = asyncStackTrace; trace; trace = trace.parent) {
      testRunner.log(`    [${trace.description}]`);
      trace.callFrames.forEach((frame, index) => {
        const url = scripts.get(frame.scriptId) || frame.url;
        testRunner.log(`    ${index}) ${frame.functionName} (${url}:${
            frame.lineNumber + 1})`);
      });
    }
  }

  const pause1Promise = dp.Debugger.oncePaused();
  await dp.Runtime.evaluate(
      {expression: 'setTimeout(testFunction, 0)', objectGroup: 'console'});
  testRunner.log('Set timer for test function.');

  const {params: pause1} = await pause1Promise;
  const {result: {scriptSource}} = await dp.Debugger.getScriptSource({
    scriptId: pause1.callFrames[0].location.scriptId,
  });
  if (!scriptSource.includes('var dummy = 42;')) {
    testRunner.log('FAIL: unexpected paused script source');
  }
  logStackTrace(pause1);

  testRunner.log('\nExecuting StepInto...');
  const pause2Promise = dp.Debugger.oncePaused();
  await dp.Debugger.stepInto({breakOnAsyncCall: false, skipList: []});
  await pause2Promise;

  testRunner.log('Executing StepInto...');
  const pause3Promise = dp.Debugger.oncePaused();
  await dp.Debugger.stepInto({breakOnAsyncCall: false, skipList: []});
  const {params: pause3} = await pause3Promise;
  logStackTrace(pause3);

  await dp.Debugger.setBreakpointsActive({active: true});
  // Skip the sentinel debugger statement in FAIL_Should_Not_Pause_Here during
  // teardown.
  await dp.Debugger.setSkipAllPauses({skip: true});
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;

  testRunner.completeTest();
});
