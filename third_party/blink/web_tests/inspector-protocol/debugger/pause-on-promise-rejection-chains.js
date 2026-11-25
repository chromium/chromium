// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that pause on promise rejection works.');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

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

function testFunction() {
  var resolved = createPromise();
  var caught = createPromise();
  var uncaught = createPromise();

  caught.promise
      .then(function c1() {})
      .then(function c2() {})
      .catch(function c3() {});
  uncaught.promise
      .then(function f1() {})
      .then(function f2() {})
      .then(function f3() {});  // Last is uncaught.

  resolved.resolve(42);  // Should not pause.
  caught.reject(new Error("caught"));
  uncaught.reject(new Error("uncaught"));
}

function scheduleTestFunction() {
  setTimeout(testFunction, 0);
}
//# sourceURL=pause-on-promise-rejection-chains.js`);

  let remainingExpectedPauses = 0;
  const pauseQueue = [];
  let pauseWaiter = null;

  dp.Debugger.onPaused(event => {
    if (remainingExpectedPauses <= 0) {
      void dp.Debugger.resume();
      testRunner.fail('Unexpected pause: ' + JSON.stringify({
        reason: event.params.reason,
        data: event.params.data,
      }));
      return;
    }
    remainingExpectedPauses--;
    if (pauseWaiter) {
      const resolve = pauseWaiter;
      pauseWaiter = null;
      resolve(event.params);
      return;
    }
    pauseQueue.push(event.params);
  });

  function nextPause() {
    if (pauseQueue.length > 0) {
      return Promise.resolve(pauseQueue.shift());
    }
    return new Promise(resolve => {
      pauseWaiter = resolve;
    });
  }

  function formatPause(params) {
    const lines = ['Call stack:'];
    params.callFrames.forEach((frame, index) => {
      const url = scripts.get(frame.location.scriptId) || '<unknown>';
      lines.push(`    ${index}) ${frame.functionName || '(anonymous)'} (${
          url}:${frame.location.lineNumber + 1})`);
    });
    for (let st = params.asyncStackTrace; st; st = st.parent) {
      lines.push(`    [${st.description || 'Async Call'}]`);
      st.callFrames.forEach((frame, index) => {
        const url = scripts.get(frame.scriptId) || frame.url || '<unknown>';
        lines.push(`    ${index}) ${frame.functionName || '(anonymous)'} (${
            url}:${frame.lineNumber + 1})`);
      });
    }
    const description = (params.data?.description || '').split('\n')[0];
    lines.push(`Paused reason: ${params.reason}, uncaught: ${
                   params.data?.uncaught}, ` +
        `description: ${description}`);
    return lines.join('\n');
  }

  async function runPhase(label, state, expectedPauses) {
    await dp.Debugger.setPauseOnExceptions({state});
    testRunner.log(label);
    remainingExpectedPauses = expectedPauses;
    await session.evaluate('scheduleTestFunction()');
    testRunner.log('Set timer for test function.');
    for (let i = 0; i < expectedPauses; ++i) {
      const params = await nextPause();
      testRunner.log(formatPause(params));
      await dp.Debugger.resume();
    }
    await session.evaluateAsync(
        'new Promise(resolve => setTimeout(resolve, 0))');
  }

  await runPhase('=== Pausing only on uncaught exceptions ===', 'uncaught', 1);
  await runPhase('\n=== Pausing on all exceptions ===', 'all', 2);

  await dp.Debugger.setPauseOnExceptions({state: 'none'});
  testRunner.completeTest();
});
