// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<script src="${testRunner.url('resources/framework.js')}"></script>`,
      'Tests Debugger.setAsyncCallStackDepth and Debugger.setBlackboxPatterns working together.');

  // Debugger.CallFrame.url is deprecated; resolve URLs via scriptParsed.
  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });
  await dp.Debugger.enable();

  await session.evaluate(`${'\n'.repeat(17)}
      window.callbackFromFramework = function(next)
      {
          return next();
      }

      function testFunction()
      {
          setTimeout(timeout1, 0);
      }

      function timeout1()
      {
          Framework.safeRun(Framework.empty, callback1);
      }

      function callback1()
      {
          Framework.doSomeAsyncChainCalls(callback2);
      }

      function callback2()
      {
          debugger;
      }
      //# sourceURL=frameworks-with-async-callstack.js
  `);

  const frameworkRegex = /\/framework\.js$/;
  await dp.Debugger.setBlackboxPatterns({patterns: [frameworkRegex.source]});
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 8});

  const pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate(`
    setTimeout(testFunction, 0);
    //# sourceURL=schedule-test-function.js
  `);
  const {params: {callFrames, asyncStackTrace}} = await pausedPromise;

  function formatStackTrace(dropFramework) {
    const results = ['Call stack:'];
    // Handles both Debugger.CallFrame (with `location`) and Runtime.CallFrame.
    function printFrames(frames) {
      let printed = 0;
      for (const frame of frames) {
        const {scriptId, lineNumber} = frame.location || frame;
        const url = scripts.get(scriptId) || '';
        const isFramework = frameworkRegex.test(url);
        if (dropFramework && isFramework) {
          continue;
        }
        const prefix = isFramework ? '  * ' : '    ';
        results.push(`${prefix}${printed++}) ${
            frame.functionName ||
            '(anonymous)'} (${url.split('/').pop()}:${lineNumber + 1})`);
      }
      return printed;
    }

    printFrames(callFrames);
    for (let st = asyncStackTrace; st; st = st.parent) {
      results.push(`    [${st.description || 'Async Call'}]`);
      if (!printFrames(st.callFrames)) {
        results.pop();
      }
    }
    return results.join('\n');
  }

  testRunner.log(formatStackTrace(false));
  testRunner.log('\nPrinting visible call stack:');
  testRunner.log(formatStackTrace(true));

  await dp.Debugger.resume();
  testRunner.completeTest();
});
