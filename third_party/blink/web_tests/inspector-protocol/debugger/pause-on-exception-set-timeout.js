// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that pause on exception works inside setTimeout callbacks.');

  // Debugger.CallFrame.url is deprecated; resolve URLs via scriptParsed.
  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await session.evaluate(`
      function throwAnException()
      {
          return unknown_var;
      }

      function handleClick()
      {
          throwAnException();
      }
      //# sourceURL=pause-on-exception-set-timeout.js`);

  function fileNameForScript(scriptId, fallbackUrl) {
    const url = scripts.get(scriptId) || fallbackUrl || '';
    return url.split('/').pop();
  }

  function logCallStack(callFrames, asyncStackTrace) {
    testRunner.log('Call stack:');
    callFrames.forEach((frame, index) => {
      const file = fileNameForScript(frame.location.scriptId);
      const line = frame.location.lineNumber + 1;
      testRunner.log(`    ${index}) ${frame.functionName} (${file}:${line})`);
    });
    for (let st = asyncStackTrace; st; st = st.parent) {
      testRunner.log(`    [${st.description}]`);
      st.callFrames.forEach((frame, index) => {
        const file = fileNameForScript(frame.scriptId, frame.url);
        const line = frame.lineNumber + 1;
        testRunner.log(`    ${index}) ${frame.functionName || '(anonymous)'} (${
            file}:${line})`);
      });
    }
  }

  await dp.Debugger.setPauseOnExceptions({state: 'uncaught'});

  const pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate(
      'setTimeout(handleClick, 0);\n//# sourceURL=schedule-set-timeout.js');
  const {params: pausedParams} = await pausedPromise;

  testRunner.log('Script execution paused.');
  testRunner.log(`Reason: ${pausedParams.reason}`);
  testRunner.log(`Uncaught: ${pausedParams.data.uncaught}`);
  const exceptionLine = (pausedParams.data.description || '').split('\n')[0];
  testRunner.log(`Exception: ${exceptionLine}`);
  logCallStack(pausedParams.callFrames, pausedParams.asyncStackTrace);

  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;
  testRunner.log('Script execution resumed.');

  await dp.Debugger.setPauseOnExceptions({state: 'none'});
  testRunner.completeTest();
});
