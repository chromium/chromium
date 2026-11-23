// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div id="outer" style="width: 100px; height: 100px; overflow:auto">
          <div id="inner" style="width: 200px; height: 200px;"></div>
      </div>`,
      'Tests asynchronous call stacks for scripted scroll events.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });
  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await session.evaluate(`
      function testFunction()
      {
          setTimeout(timeout, 0);
      }

      function timeout()
      {
          var outer = document.getElementById("outer");
          outer.scrollTop = 0;
          outer.addEventListener("scroll", onScroll1, false);
          outer.addEventListener("scroll", onScroll2, false);
          outer.scrollTop = 40;
          outer.scrollTop = 60;
      }

      function onScroll1()
      {
          var outer = document.getElementById("outer");
          outer.removeEventListener("scroll", onScroll1, false);
          debugger;
      }

      function onScroll2()
      {
          var outer = document.getElementById("outer");
          outer.removeEventListener("scroll", onScroll2, false);
          debugger;
      }
      //# sourceURL=async-callstack-scripted-scroll.js
  `);

  function formatStackTrace(callFrames, asyncStackTrace) {
    const lines = ['Call stack:'];
    function appendFrames(frames) {
      let printed = 0;
      for (let i = 0; i < frames.length; ++i) {
        const frame = frames[i];
        const {scriptId, lineNumber} = frame.location || frame;
        const url = scripts.get(scriptId) || frame.url || '';
        const name = frame.functionName || '(anonymous)';
        lines.push(`    ${printed++}) ${name} (${url}:${lineNumber + 1})`);
        if (name === 'scheduleTestFunction') {
          if (i < frames.length - 1) {
            lines.push('    <... skipped remaining frames ...>');
          }
          break;
        }
      }
      return printed;
    }

    appendFrames(callFrames);
    for (let st = asyncStackTrace; st; st = st.parent) {
      lines.push(`    [${st.description || 'Async Call'}]`);
      if (!appendFrames(st.callFrames)) {
        lines.pop();
      }
    }
    return lines.join('\n') + '\n';
  }

  const totalDebuggerStatements = 2;
  const callStacksOutput = [];
  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate(`
      function scheduleTestFunction()
      {
          setTimeout(testFunction, 0);
      }
      scheduleTestFunction();
      //# sourceURL=schedule-test-function.js
  `);

  for (let step = 0; step < totalDebuggerStatements; ++step) {
    const {params: {callFrames, asyncStackTrace}} = await pausedPromise;
    callStacksOutput.push(formatStackTrace(callFrames, asyncStackTrace));
    if (step + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
    }
    await dp.Debugger.resume();
  }

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  for (const output of callStacksOutput) {
    testRunner.log(output);
  }
  testRunner.completeTest();
});
