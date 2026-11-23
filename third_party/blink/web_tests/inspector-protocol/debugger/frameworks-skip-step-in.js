// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests the skip stack frames feature when stepping.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });
  await dp.Debugger.enable();

  await session.evaluate(`
var Framework = {};

Framework.safeRun = function(callback, onSuccess, onException, breakOnUncaught)
{
    try {
        callback();
        if (onSuccess)
            Framework.safeRun(onSuccess, undefined, onException, breakOnUncaught);
    } catch (e) {
        if (onException)
            Framework.safeRun(onException, undefined, breakOnUncaught ? Framework.breakInFramework : undefined);
        else if (breakOnUncaught)
            Framework.breakInFramework();
    }
}

Framework.throwFrameworkException = function(msg)
{
    throw Error("FrameworkException" + (msg ? ": " + msg : ""));
}

Framework.breakInFramework = function()
{
    debugger;
}

Framework.empty = function()
{
}

Framework.doSomeWork = function()
{
    const numberOfSteps = 50;
    for (var i = 0; i < numberOfSteps; ++i) {
        if (window["dummy property should not exist!" + i]) // Prevent optimizations.
            return i;
        Framework.safeRun(Framework.empty, Framework.empty, Framework.empty, true);
    }
}

Framework.doSomeWorkDoNotChangeTopCallFrame = function()
{
    const numberOfSteps = 5000;
    for (var i = 0; i < numberOfSteps; ++i) {
        if (window["dummy property should not exist!" + i]) // Prevent optimizations.
            return i;
    }
    return -1;
}
//# sourceURL=framework.js
  `);

  await session.evaluate(`
function testFunction()
{
    for (var i = 1, func; func = eval("typeof test" + i + " === 'function' && test" + i); ++i)
        func();
}

function callback()
{
    return 0;
}

function test1()
{
    debugger;
    Framework.empty();
}

function test2()
{
    debugger;
    Framework.doSomeWork();
}

function test3()
{
    debugger;
    Framework.safeRun(Framework.empty, callback); // Should step into callback
}

function test4()
{
    debugger;
    Framework.safeRun(Framework.doSomeWork, callback); // Should step into callback
}

function test5()
{
    debugger;
    // Should be enough to step into callback.
    Framework.safeRun(Framework.empty, Framework.throwFrameworkException, callback);
}

function test6()
{
    debugger;
    Framework.safeRun(Framework.doSomeWorkDoNotChangeTopCallFrame, callback);
}
//# sourceURL=frameworks-skip-step-in.js
  `);

  const frameworkRegex = /framework\.js$/;
  const totalDebuggerStatements = 6;
  await dp.Debugger.setBlackboxPatterns({patterns: [frameworkRegex.source]});

  function dumpCallStack(callFrames) {
    testRunner.log('Call stack:');
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const url = scripts.get(frame.location.scriptId) || '';
      const isFramework = frameworkRegex.test(url);
      if (i === 0 && isFramework) {
        testRunner.log(
            `FAIL: paused in blackboxed frame ${frame.functionName}`);
      }
      const prefix = isFramework ? '  * ' : '    ';
      const fileName = url.split('/').pop();
      testRunner.log(`${prefix}${i}) ${frame.functionName} (${fileName}:${
          frame.location.lineNumber + 1})`);
    }
  }

  testRunner.log('Set timer for test function.');
  let pausedPromise = dp.Debugger.oncePaused();
  await session.evaluate('setTimeout(testFunction, 0)');

  for (let step = 0; step < totalDebuggerStatements; ++step) {
    let {params: {callFrames}} = await pausedPromise;
    for (let stepInCount = 0; stepInCount < 2; ++stepInCount) {
      pausedPromise = dp.Debugger.oncePaused();
      await dp.Debugger.stepInto();
      ({params: {callFrames}} = await pausedPromise);
    }
    dumpCallStack(callFrames);
    testRunner.log('');
    if (step + 1 < totalDebuggerStatements) {
      pausedPromise = dp.Debugger.oncePaused();
      await dp.Debugger.resume();
    } else {
      await dp.Debugger.resume();
    }
  }

  testRunner.completeTest();
});
