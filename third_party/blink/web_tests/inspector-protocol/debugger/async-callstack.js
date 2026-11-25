// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks in debugger.');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  const consoleLogs = [];
  dp.Runtime.onConsoleAPICalled(({params}) => {
    const text = params.args.map(arg => arg.value ?? arg.description).join(' ');
    if (params.type === 'log' && text === 'animFrame1') {
      consoleLogs.push(text);
    } else {
      testRunner.fail(`Unexpected console ${params.type}: ${text}`);
    }
  });
  dp.Runtime.onExceptionThrown(({params}) => {
    testRunner.fail(`Unexpected exception: ${params.exceptionDetails.text}`);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  await session.evaluate(`
    function testFunction()
    {
        function innerTestFunction()
        {
            timeout1();
        }
        setTimeout(innerTestFunction, 0);
    }

    function timeout1()
    {
        debugger;
        requestAnimationFrame(animFrame1);
        var id = setInterval(innerInterval1, 0);
        function innerInterval1()
        {
            clearInterval(id);
            interval1();
        }
    }

    function animFrame1()
    {
        console.log('animFrame1');
        debugger;
        setTimeout(timeout2, 0);
        requestAnimationFrame(animFrame2);
    }

    function interval1()
    {
        debugger;
    }

    function timeout2()
    {
        debugger;
    }

    function animFrame2()
    {
        debugger;
        function longTail0()
        {
            timeout3();
        }
        var funcs = [];
        for (var i = 0; i < 20; ++i)
            funcs.push("function longTail" + (i + 1) + "() { setTimeout(longTail" + i + ", 0); };");
        funcs.push("setTimeout(longTail" + i + ", 0);");
        eval(funcs.join("\\n"));
    }

    function timeout3()
    {
        debugger;
    }
    //# sourceURL=async-callstack.js
  `);

  function formatStackTrace(callFrames, asyncStackTrace, asyncStackTraceId) {
    const results = ['Call stack:'];
    function printFrames(frames) {
      let printed = 0;
      for (const frame of frames) {
        const {scriptId, lineNumber} = frame.location || frame;
        const url = scripts.get(scriptId) || frame.url || '';
        const source = url ? url.split('/').pop() : '';
        results.push(`    ${printed++}) ${
            frame.functionName ||
            '(anonymous)'} (${source}:${lineNumber + 1})`);
      }
    }
    printFrames(callFrames);
    if (asyncStackTraceId) {
      testRunner.fail(
          'Unexpected asyncStackTraceId: expected inline asyncStackTrace within maxDepth 32');
      return '';
    }
    for (let st = asyncStackTrace; st; st = st.parent) {
      results.push(`    [${st.description || 'Async Call'}]`);
      printFrames(st.callFrames);
      if (!st.parent && st.parentId) {
        testRunner.fail(
            'Unexpected st.parentId: expected inline parent chain within maxDepth 32');
        break;
      }
    }
    return results.join('\n') + '\n';
  }

  let pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate(`
    setTimeout(testFunction, 0);
    //# sourceURL=schedule-test-function.js
  `);

  const totalDebuggerStatements = 6;
  const callStacksOutput = [];
  for (let i = 0; i < totalDebuggerStatements; ++i) {
    const {params: {callFrames, asyncStackTrace, asyncStackTraceId}} =
        await pausedPromise;
    callStacksOutput.push(
        formatStackTrace(callFrames, asyncStackTrace, asyncStackTraceId));
    if (i + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
    }
    await dp.Debugger.resume();
  }

  let unexpectedPauseCount = 0;
  dp.Debugger.onPaused(async () => {
    unexpectedPauseCount++;
    await dp.Debugger.resume();
  });
  await session.evaluateAsync(
      'new Promise(resolve => requestAnimationFrame(() => setTimeout(resolve, 0)))');
  if (unexpectedPauseCount !== 0) {
    testRunner.fail(
        `Unexpected pauses after completion: ${unexpectedPauseCount}`);
  }
  if (consoleLogs.length !== 1 || consoleLogs[0] !== 'animFrame1') {
    testRunner.fail(`Expected console.log('animFrame1') once, got ${
        JSON.stringify(consoleLogs)}`);
  }

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  for (const output of callStacksOutput) {
    testRunner.log(output);
  }
  testRunner.completeTest();
})
