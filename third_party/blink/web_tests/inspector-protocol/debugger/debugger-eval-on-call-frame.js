// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Test that evaluation in the context of top frame will see values of its local variables, even if there are global variables with same names. On success the test will print a = 2(value of the local variable a). Bug 47358.\n`);

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  await dp.Runtime.evaluate({
    expression: `
      var a = 1;
      function testFunction()
      {
          var a = 2;
          debugger;
      }
  `,
    objectGroup: 'console',
  });

  const pausedPromise = dp.Debugger.oncePaused();
  const resumedPromise = dp.Debugger.onceResumed();

  await dp.Runtime.evaluate({
    expression: 'setTimeout(testFunction, 0)',
    objectGroup: 'console',
  });
  testRunner.log('Set timer for test function.');

  const {params: {callFrames}} = await pausedPromise;
  testRunner.log('Script execution paused.');

  const {result: {result}} = await dp.Debugger.evaluateOnCallFrame({
    callFrameId: callFrames[0].callFrameId,
    expression: 'a',
    objectGroup: 'console',
    includeCommandLineAPI: true,
    silent: false,
    returnByValue: false,
    generatePreview: true,
  });
  testRunner.log('Evaluated in console in the top frame context: a = ' +
                 result.description);

  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;
  testRunner.log('Script execution resumed.');

  testRunner.completeTest();
});
