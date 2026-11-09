// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Debugger.setAsyncCallStackDepth captures asynchronous call stacks when enabled after the page loads.');

  await dp.Debugger.enable();
  await session.evaluate(`${'\n'.repeat(11)}
      function testFunction()
      {
          setTimeout(timeoutOffCapturing, 0);
      }

      function timeoutOffCapturing()
      {
          setTimeout(timeoutOffCapturing2, 0);
          debugger;
          setTimeout(timeoutOnCapturing, 0);
      }

      function timeoutOffCapturing2()
      {
          debugger;
      }

      function timeoutOnCapturing()
      {
          debugger;
      }
      //# sourceURL=async-callstack-middle-run.js
  `);

  function formatStackTrace(callFrames, asyncStackTrace) {
    const lines = ['Call stack:'];
    callFrames.forEach((f, i) => {
      lines.push(`    ${i}) ${f.functionName} (async-callstack-middle-run.js:${
          f.location.lineNumber + 1})`);
    });
    for (let st = asyncStackTrace; st; st = st.parent) {
      lines.push(`    [${st.description}]`);
      st.callFrames.forEach((f, i) => {
        lines.push(
            `    ${i}) ${f.functionName} (async-callstack-middle-run.js:${
                f.lineNumber + 1})`);
      });
    }
    return lines.join('\n') + '\n';
  }

  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 0});
  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate('setTimeout(testFunction, 0)');
  await pausedPromise;

  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 8});

  const callStacksOutput = [];
  for (let step = 2; step <= 3; ++step) {
    pausedPromise = dp.Debugger.oncePaused();
    await dp.Debugger.resume();
    const {params: {callFrames, asyncStackTrace}} = await pausedPromise;
    callStacksOutput.push(formatStackTrace(callFrames, asyncStackTrace));
  }
  await dp.Debugger.resume();

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  for (const output of callStacksOutput) {
    testRunner.log(output);
  }
  testRunner.completeTest();
});
