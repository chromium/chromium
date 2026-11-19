// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that debugger will stop on "debugger" statement w/ overriden string, etc.\n');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });
  await dp.Runtime.enable();
  await dp.Debugger.enable();

  await dp.Runtime.evaluate({
    expression: `${'\n'.repeat(11)}
      function testFunction()
      {
          String = "aa";
          debugger;
      }
      //# sourceURL=debugger-pause-with-overrides.js`,
    objectGroup: 'console',
  });

  const pausedPromise = dp.Debugger.oncePaused();
  await dp.Runtime.evaluate(
      {expression: 'setTimeout(testFunction, 0)', objectGroup: 'console'});
  testRunner.log('Set timer for test function.');

  const {params: {callFrames}} = await pausedPromise;
  testRunner.log('Script execution paused.');

  const topScriptId = callFrames[0].location.scriptId;
  const {result: {scriptSource}} =
      await dp.Debugger.getScriptSource({scriptId: topScriptId});
  if (!scriptSource.includes('String = "aa";')) {
    testRunner.log('FAIL: unexpected paused script source');
  }

  testRunner.log('Call stack:');
  callFrames.forEach((frame, index) => {
    const url = scripts.get(frame.location.scriptId) || '';
    testRunner.log(`    ${index}) ${frame.functionName} (${
        url.split('/').pop()}:${frame.location.lineNumber + 1})`);
  });

  await dp.Debugger.setBreakpointsActive({active: true});
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;
  testRunner.log('Script execution resumed.');
  testRunner.completeTest();
});
