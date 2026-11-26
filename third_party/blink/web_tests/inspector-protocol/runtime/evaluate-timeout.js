// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests timeout support for Runtime.evaluate and ' +
      'Debugger.evaluateOnCallFrame.');

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const regularExpression = '1 + 1';
  const infiniteExpression = 'while (1){}';

  function printDetails(response) {
    if (response.error) {
      testRunner.log(`Error: ${response.error.message}`);
      return;
    }
    const {result, exceptionDetails} = response.result;
    if (exceptionDetails) {
      testRunner.log(`Exception: ${exceptionDetails.text}`);
      return;
    }
    testRunner.log('Result:');
    testRunner.log(`  Description: ${result.description}`);
    testRunner.log(`  Value:       ${result.value}`);
    testRunner.log(`  Type:        ${result.type}`);
  }

  async function runtimeTestCase(expression, timeout) {
    testRunner.log(
        `\nTesting expression ${expression} with timeout: ${timeout}`);
    const response = await dp.Runtime.evaluate({expression, timeout});
    printDetails(response);
  }

  async function debuggerTestCase(callFrameId, expression, timeout) {
    testRunner.log(
        `\nTesting expression ${expression} with timeout: ${timeout}`);
    const response = await dp.Debugger.evaluateOnCallFrame(
        {callFrameId, expression, timeout});
    printDetails(response);
  }

  await runtimeTestCase(infiniteExpression, 0);
  await runtimeTestCase(regularExpression);

  // Debugger evaluateOnCallFrame test.
  await session.evaluate(`
    function testFunction()
    {
        debugger;
    }
  `);

  const pausedPromise = dp.Debugger.oncePaused();
  testRunner.log('Set timer for test function.');
  await session.evaluate('setTimeout(testFunction, 0)');
  const {params: {callFrames}} = await pausedPromise;
  testRunner.log('Script execution paused.');

  const callFrameId = callFrames[0].callFrameId;
  await debuggerTestCase(callFrameId, infiniteExpression, 0);
  await debuggerTestCase(callFrameId, regularExpression);

  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;
  testRunner.log('Script execution resumed.');

  testRunner.completeTest();
});
