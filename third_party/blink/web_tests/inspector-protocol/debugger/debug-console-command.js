// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests debug(fn) console command.');

  // Debugger.CallFrame.url is deprecated; resolve URLs via scriptParsed.
  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });
  await dp.Debugger.enable();

  await session.evaluate(`
function simpleTestFunction()
{
   return 0;
}

function simpleTestFunction1() { return 0; } function simpleTestFunction2() { return 0; }

function simpleTestFunction3() { Math.random(); debugger; }
//# sourceURL=debug-console-command.js`);

  function logEvaluationError(response) {
    if (response.error) {
      testRunner.log('Protocol error: ' + JSON.stringify(response.error));
    } else if (response.result?.exceptionDetails) {
      testRunner.log('Exception: ' +
                     JSON.stringify(response.result.exceptionDetails.text));
    }
  }

  function logCallStack(callFrames) {
    testRunner.log('Call stack:');
    callFrames.forEach((frame, index) => {
      const fileName =
          (scripts.get(frame.location.scriptId) || '').split('/').pop();
      testRunner.log(`    ${index}) ${frame.functionName} (${fileName}:${
          frame.location.lineNumber + 1})`);
    });
  }

  async function setBreakpointAndRun(functionName, runCmd) {
    logEvaluationError(await dp.Runtime.evaluate({
      expression: `debug(${functionName})`,
      includeCommandLineAPI: true,
    }));
    testRunner.log('Breakpoint added.');

    const pausedPromise = dp.Debugger.oncePaused();
    logEvaluationError(await dp.Runtime.evaluate({
      expression: `setTimeout(function() { ${runCmd} }, 20)`,
      includeCommandLineAPI: true,
    }));
    testRunner.log('Set timer for test function.');

    const {params: pausedParams} = await pausedPromise;
    testRunner.log('Script execution paused.');
    testRunner.log(`Reason for pause: ${pausedParams.reason}`);
    logCallStack(pausedParams.callFrames);

    logEvaluationError(await dp.Debugger.evaluateOnCallFrame({
      callFrameId: pausedParams.callFrames[0].callFrameId,
      expression: `undebug(${functionName})`,
      includeCommandLineAPI: true,
    }));
    testRunner.log('Breakpoint removed.');

    const resumedPromise = dp.Debugger.onceResumed();
    await dp.Debugger.resume();
    await resumedPromise;
    testRunner.log('Script execution resumed.');
  }

  testRunner.runTestSuite([
    async function testSetSimpleBreakpoint() {
      await setBreakpointAndRun('simpleTestFunction', 'simpleTestFunction();');
    },

    async function testSetBreakpointOnFirstFunctionInLine() {
      await setBreakpointAndRun(
          'simpleTestFunction1',
          'simpleTestFunction2(); simpleTestFunction1();');
    },

    async function testSetBreakpointOnLastFunctionInLine() {
      await setBreakpointAndRun(
          'simpleTestFunction2',
          'simpleTestFunction1(); simpleTestFunction2();');
    },

    async function testRemoveBreakpoint() {
      logEvaluationError(await dp.Runtime.evaluate({
        expression: 'debug(simpleTestFunction3); undebug(simpleTestFunction3);',
        includeCommandLineAPI: true,
      }));
      const pausedPromise = dp.Debugger.oncePaused();
      logEvaluationError(await dp.Runtime.evaluate({
        expression: 'setTimeout(simpleTestFunction3, 0)',
        includeCommandLineAPI: true,
      }));

      const {params: pausedParams} = await pausedPromise;
      testRunner.log('Script execution paused.');
      const reasonText = pausedParams.reason === 'debugCommand' ?
          'debug command' :
          pausedParams.reason === 'other' ? 'debugger statement' :
                                            pausedParams.reason;
      testRunner.log(`Reason for pause: ${reasonText}.`);

      const resumedPromise = dp.Debugger.onceResumed();
      await dp.Debugger.resume();
      await resumedPromise;
    },
  ]);
})
