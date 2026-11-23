// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that debugger StepOut will stop inside next timeout handler.');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

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
          return 1;
      }

      function callback2()
      {
          var dummy = 42; // Should pause here.
          (function FAIL_Should_Not_Pause_Here() { debugger; })();
      }
      //# sourceURL=debugger-step-out-across-timeouts.js
    `,
    objectGroup: 'console',
  });

  function formatStackTrace(callFrames, asyncStackTrace) {
    const lines = ['Call stack:'];
    callFrames.forEach((f, i) => {
      const url =
          (scripts.get(f.location.scriptId) || f.url || '').split('/').pop();
      lines.push(`    ${i}) ${f.functionName || '(anonymous)'} (${url}:${
          f.location.lineNumber + 1})`);
    });
    for (let st = asyncStackTrace; st; st = st.parent) {
      lines.push(`    [${st.description || 'Async Call'}]`);
      st.callFrames.forEach((f, i) => {
        const url = (scripts.get(f.scriptId) || f.url || '').split('/').pop();
        lines.push(`    ${i}) ${f.functionName || '(anonymous)'} (${url}:${
            f.lineNumber + 1})`);
      });
    }
    return lines.join('\n');
  }

  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await dp.Runtime.evaluate({
    expression: `
      setTimeout(testFunction, 0);
      //# sourceURL=schedule-test-function.js
    `,
    objectGroup: 'console',
  });

  const pause1 = await pausedPromise;
  testRunner.log(formatStackTrace(pause1.params.callFrames,
                                  pause1.params.asyncStackTrace));
  testRunner.log('');

  testRunner.log('Executing StepOut...');
  pausedPromise = dp.Debugger.oncePaused();
  await dp.Debugger.stepOut();
  const pause2 = await pausedPromise;
  testRunner.log(formatStackTrace(pause2.params.callFrames,
                                  pause2.params.asyncStackTrace));

  await dp.Debugger.disable();
  testRunner.completeTest();
});
