// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<input type="button" onclick="testFunction()" value="Test">`,
      'Tests that debugger will step through Promise handlers while not stepping into V8 internal scripts.');

  await dp.Runtime.enable();
  const debuggerId = (await dp.Debugger.enable()).result.debuggerId;
  const debuggers = new Map([[debuggerId, dp.Debugger]]);
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  dp.Runtime.onConsoleAPICalled(({params}) => {
    testRunner.log(
        `Console ${params.type}: ${params.args.map(a => a.value).join(' ')}`);
  });
  dp.Runtime.onExceptionThrown(({params}) => {
    testRunner.log(`Exception: ${params.exceptionDetails.text}`);
  });

  const scriptParsedPromise = dp.Debugger.onceScriptParsed(
      ({params}) => params.url === 'step-through-promises.js');
  await session.evaluate(`
      function testFunction()
      {
          Promise.resolve(42).then(
              function p1()
              {
                  debugger;
              }
          ).then(
              function p2()
              {
                  return window.foo || 1;
              }
          ).then(
              function p3()
              {
                  return window.foo || 2;
              }
          ).catch(function(e) {
              console.error("FAIL: Unexpected exception: " + e);
          });
      }
      //# sourceURL=step-through-promises.js
  `);
  const {params: {scriptId}} = await scriptParsedPromise;
  const {result: {scriptSource}} =
      await dp.Debugger.getScriptSource({scriptId});
  const sourceLines = scriptSource.split('\n');

  async function logStackTrace(callFrames, asyncStackTrace, asyncStackTraceId) {
    let stackTrace = {
      callFrames,
      parent: asyncStackTrace,
      parentId: asyncStackTraceId,
    };
    let currentDebuggerId = debuggerId;
    while (stackTrace) {
      const {description, callFrames: frames, parent, parentId} = stackTrace;
      if (description) {
        testRunner.log(`    [${description}]`);
      }
      for (let i = 0; i < frames.length; ++i) {
        const frame = frames[i];
        const fnName = frame.functionName || '(anonymous)';
        const loc = frame.location || frame;
        const url = frame.url || scripts.get(loc.scriptId) || '';
        testRunner.log(`    ${i}) ${fnName} (${url}:${loc.lineNumber}:${
            loc.columnNumber})`);
      }
      if (parentId) {
        if (parentId.debuggerId) {
          currentDebuggerId = parentId.debuggerId;
        }
        const response = await debuggers.get(currentDebuggerId).getStackTrace({
          stackTraceId: parentId,
        });
        stackTrace = response.stackTrace || response.result.stackTrace;
      } else {
        stackTrace = parent;
      }
    }
  }

  async function logPaused(pauseParams) {
    const {callFrames, asyncStackTrace, asyncStackTraceId} = pauseParams;
    const topLocation = callFrames[0].location;
    const scriptUrl = scripts.get(topLocation.scriptId) || '<unknown>';
    let markedLine = '<external>';
    if (topLocation.scriptId === scriptId &&
        topLocation.lineNumber < sourceLines.length) {
      const rawLine = sourceLines[topLocation.lineNumber];
      markedLine = (rawLine.slice(0, topLocation.columnNumber) + '#' +
                    rawLine.slice(topLocation.columnNumber))
                       .trim();
    }
    testRunner.log(`Paused at ${scriptUrl}:${topLocation.lineNumber}:${
        topLocation.columnNumber}: ${markedLine}`);
    testRunner.log('Call stack:');
    await logStackTrace(callFrames, asyncStackTrace, asyncStackTraceId);
  }

  testRunner.log('');
  testRunner.log('Set timer for test function.');
  let pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0)');
  let paused = await pausedPromise;
  await logPaused(paused.params);

  const actions = [
    'stepInto',
    'stepInto',
    'stepOver',
    'stepOver',
    'stepOver',
  ];
  for (const action of actions) {
    testRunner.log('');
    testRunner.log(`Executing ${action}...`);
    pausedPromise = dp.Debugger.oncePaused();
    await dp.Debugger[action]();
    paused = await pausedPromise;
    await logPaused(paused.params);
  }

  dp.Debugger.onPaused(() => {
    testRunner.log('FAIL: Unexpected pause after resume.');
  });
  await dp.Debugger.resume();
  await session.evaluateAsync('new Promise(resolve => setTimeout(resolve, 0))');

  testRunner.completeTest();
});
