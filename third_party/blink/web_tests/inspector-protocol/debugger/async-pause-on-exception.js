// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that pause on promise rejection works.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 8});

  await session.evaluate(`
    function createPromise() {
      var result = {};
      var p = new Promise(function(resolve, reject) {
        result.resolve = resolve;
        result.reject = reject;
      });
      result.promise = p;
      return result;
    }

    async function asyncCaught(promise) {
      try {
        await promise;
      } catch (e) { }
    }

    async function asyncUncaught(promise) {
      await promise;
    }

    function testFunction() {
      var caught = createPromise();
      var uncaught = createPromise();

      asyncCaught(caught.promise);
      asyncUncaught(uncaught.promise);

      caught.reject(new Error("caught"));
      uncaught.reject(new Error("uncaught"));
    }
    //# sourceURL=async-pause-on-exception.js
  `);

  function formatLocation(scriptId, lineNumber) {
    const url = (scripts.get(scriptId) || '').split('/').pop();
    return `${url}:${lineNumber + 1}`;
  }

  function logPause(params) {
    testRunner.log('Call stack:');
    params.callFrames.forEach((frame, index) => {
      testRunner.log(`    ${index}) ${frame.functionName} (${
          formatLocation(frame.location.scriptId,
                         frame.location.lineNumber)})`);
    });
    for (let st = params.asyncStackTrace; st; st = st.parent) {
      testRunner.log(`    [${st.description}]`);
      st.callFrames.forEach((frame, index) => {
        const name = frame.functionName || '(anonymous)';
        testRunner.log(`    ${index}) ${name} (${
            formatLocation(frame.scriptId, frame.lineNumber)})`);
      });
    }
    const description = (params.data?.description || '').split('\n')[0];
    testRunner.log(`Paused on ${params.reason} (uncaught: ${
        params.data?.uncaught}): ${description}`);
  }

  let onPauseCallback = null;
  dp.Debugger.onPaused(message => {
    if (onPauseCallback) {
      const callback = onPauseCallback;
      onPauseCallback = null;
      callback(message.params);
    } else {
      testRunner.log(`FAIL: unexpected pause: ${message.params.reason}`);
      dp.Debugger.resume({terminateOnResume: false});
    }
  });

  function waitForPause() {
    return new Promise(resolve => {
      onPauseCallback = resolve;
    });
  }

  async function runTestFunctionAndExpectPauses(expectedCount) {
    testRunner.log('Set timer for test function.');
    let pausePromise = expectedCount > 0 ? waitForPause() : null;
    await session.evaluate(
        'setTimeout(testFunction, 0) //# sourceURL=schedule-test-function.js');
    for (let i = 0; i < expectedCount; ++i) {
      const paused = await pausePromise;
      logPause(paused);
      pausePromise = i + 1 < expectedCount ? waitForPause() : null;
      await dp.Debugger.resume({terminateOnResume: false});
    }
    // Drain remaining macrotasks/microtasks to confirm no extra pauses occur.
    await session.evaluateAsync(
        'new Promise(resolve => setTimeout(resolve, 0))');
  }

  await dp.Debugger.setPauseOnExceptions({state: 'uncaught'});
  testRunner.log('=== Pausing only on uncaught exceptions ===');
  await runTestFunctionAndExpectPauses(1);

  await dp.Debugger.setPauseOnExceptions({state: 'all'});
  testRunner.log('\n=== Pausing on all exceptions ===');
  await runTestFunctionAndExpectPauses(2);

  await dp.Debugger.setPauseOnExceptions({state: 'none'});
  testRunner.completeTest();
});
