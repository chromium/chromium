// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests "step out" functionality in debugger.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  await dp.Runtime.evaluate({
    expression: `${'\n'.repeat(11)}      function d()
      {
          debugger;
      }

      function testFunction()
      {
          d();
      }
  //# sourceURL=debugger-step-out.js`,
    objectGroup: 'console',
  });

  function logCallFrames(callFrames) {
    testRunner.log('Call stack:');
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const url = scripts.get(frame.location.scriptId) || frame.url;
      testRunner.log(`    ${i}) ${frame.functionName} (${url}:${
          frame.location.lineNumber + 1})`);
    }
  }

  const firstPausePromise = dp.Debugger.oncePaused();
  await dp.Runtime.evaluate({
    expression: 'setTimeout(testFunction, 0)',
    objectGroup: 'console',
  });
  testRunner.log('Set timer for test function.');

  const firstPause = await firstPausePromise;
  testRunner.log('Script execution paused.');
  await dp.Debugger.getScriptSource({
    scriptId: firstPause.params.callFrames[0].location.scriptId,
  });
  logCallFrames(firstPause.params.callFrames);

  testRunner.log('Stepping out...');
  const firstResumePromise = dp.Debugger.onceResumed();
  const secondPausePromise = dp.Debugger.oncePaused();
  await dp.Debugger.stepOut();
  await firstResumePromise;
  testRunner.log('Script execution resumed.');

  const secondPause = await secondPausePromise;
  testRunner.log('Script execution paused.');
  logCallFrames(secondPause.params.callFrames);

  await dp.Debugger.setBreakpointsActive({active: true});
  const secondResumePromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await secondResumePromise;
  testRunner.log('Script execution resumed.');

  testRunner.completeTest();
});
