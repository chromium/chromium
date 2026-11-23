// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks for async functions.');

  // Debugger.CallFrame.url is deprecated; resolve URLs via scriptParsed.
  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const args = params.args.map(arg => arg.value ?? arg.description).join(' ');
    testRunner.log(`Unexpected console.${params.type}: ${args}`);
  });
  dp.Runtime.onExceptionThrown(({params: {exceptionDetails}}) => {
    const details = exceptionDetails.exception?.description || '';
    testRunner.log(`Unexpected exception: ${exceptionDetails.text} ${details}`);
  });

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  await session.evaluate(`${'\n'.repeat(26)}
    function timeoutPromise(value, ms)
    {
      return new Promise(function promiseCallback(resolve, reject) {
        function resolvePromise()
        {
          resolve(value);
        }
        function rejectPromise()
        {
          reject(value);
        }
        if (value instanceof Error)
          setTimeout(rejectPromise, ms || 0);
        else
          setTimeout(resolvePromise, ms || 0);
      });
    }

    function settledPromise(value)
    {
      function resolveCallback(resolve, reject)
      {
        resolve(value);
      }
      function rejectCallback(resolve, reject)
      {
        reject(value);
      }
      if (value instanceof Error)
        return new Promise(rejectCallback);
      else
        return new Promise(resolveCallback);
    }

    function testFunction()
    {
      setTimeout(testFunctionTimeout, 0);
    }

    function testFunctionTimeout()
    {
      var functions = [
        doTestPromiseAll,
        doTestThrowFromChain,
        doTestRejectFromChain,
        doTestPromiseResolve,
        doTestPromiseReject,
      ];
      for (var i = 0; i < functions.length; ++i)
        functions[i]();
    }

    function thenCallback(value)
    {
      debugger;
    }

    function errorCallback(error)
    {
      debugger;
    }

    async function doTestPromiseAll()
    {
      try {
        let all = await Promise.all([11, 22, 33, 44, 55].map(timeoutPromise));
        thenCallback(all);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function throwFromChain()
    {
      await timeoutPromise(1);
      await timeoutPromise(2);
      await settledPromise(3);
      throw Error("thrown from 4");
      await timeoutPromise(5);
      debugger;
    }

    async function doTestThrowFromChain()
    {
      try {
        let result = await throwFromChain();
        thencallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function rejectFromChain()
    {
      await timeoutPromise(1);
      await timeoutPromise(2);
      await timeoutPromise(3);
      await timeoutPromise(new Error(4));
      throw new Error("thrown from rejectFromChain");
      debugger;
    }

    async function doTestRejectFromChain()
    {
      try {
        let result = await rejectFromChain();
        thenCallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function doTestPromiseResolve()
    {
      try {
        let result = await Promise.resolve(1);
        thenCallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }

    async function doTestPromiseReject()
    {
      try {
        let result = await Promise.reject(new Error("2"));
        thenCallback(result);
      } catch (e) {
        errorCallback(e);
      }
    }
    //# sourceURL=async-callstack-async-await3.js
  `);

  function formatStackTrace(callFrames, asyncStackTrace) {
    const lines = ['Call stack:'];
    function appendFrames(frames) {
      let printed = 0;
      for (const [i, frame] of frames.entries()) {
        const {scriptId, lineNumber} = frame.location || frame;
        const url =
            (scripts.get(scriptId) || '').split('/').pop() || '<unknown>';
        const name = frame.functionName || '(anonymous)';
        lines.push(`    ${i}) ${name} (${url}:${lineNumber + 1})`);
        ++printed;
      }
      return printed;
    }

    appendFrames(callFrames);
    for (let st = asyncStackTrace; st; st = st.parent) {
      lines.push(`    [${st.description || 'Async Call'}]`);
      if (!appendFrames(st.callFrames)) {
        lines.pop();
      }
      if (st.parentId) {
        lines.push(
            `FAIL: unexpected external asyncStackTraceId: ${st.parentId.id}`);
      }
    }
    return lines.join('\n');
  }

  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  const totalDebuggerStatements = 5;
  const callStacksOutput = [];
  let onNextPause = null;
  let unexpectedPauses = 0;

  dp.Debugger.onPaused(({params: {callFrames, asyncStackTrace}}) => {
    if (onNextPause) {
      const resolve = onNextPause;
      onNextPause = null;
      callStacksOutput.push(formatStackTrace(callFrames, asyncStackTrace));
      resolve();
    } else {
      ++unexpectedPauses;
      testRunner.log(`FAIL: unexpected extra pause #${unexpectedPauses}`);
      void dp.Debugger.resume();
    }
  });

  testRunner.log('Set timer for test function.');
  let nextPause = new Promise(resolve => {
    onNextPause = resolve;
  });
  await session.evaluate(`
    function scheduleTestFunction() {
      setTimeout(testFunction, 0);
    }
    scheduleTestFunction();
    //# sourceURL=schedule-test-function.js
  `);

  for (let i = 0; i < totalDebuggerStatements; ++i) {
    await nextPause;
    if (i + 1 < totalDebuggerStatements) {
      nextPause = new Promise(resolve => {
        onNextPause = resolve;
      });
    }
    await dp.Debugger.resume();
  }

  await session.evaluateAsync('new Promise(resolve => setTimeout(resolve, 0))');

  testRunner.log('Captured call stacks in no particular order:');
  callStacksOutput.sort();
  testRunner.log(callStacksOutput.join('\n\n'));
  testRunner.completeTest();
});
