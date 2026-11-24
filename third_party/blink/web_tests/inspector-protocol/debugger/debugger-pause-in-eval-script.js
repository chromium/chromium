// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests that debugger will stop on "debugger" statement in a function that was added to the inspected page via evaluation in Web Inspector console.`);

  await dp.Runtime.enable();
  await dp.Debugger.enable();

  const scriptToEvaluate = 'function testFunction() {\n' +
      '    debugger;\n' +
      '}\n' +
      'setTimeout(testFunction, 0);\n';

  const pausedPromise = dp.Debugger.oncePaused();
  await dp.Runtime.evaluate({
    expression: scriptToEvaluate,
    objectGroup: 'console',
    includeCommandLineAPI: true,
    silent: false,
    returnByValue: false,
    generatePreview: true,
    userGesture: true,
    awaitPromise: false,
    replMode: true,
  });

  const {params: {callFrames}} = await pausedPromise;
  testRunner.log('Script execution paused.');

  await dp.Debugger.getScriptSource({
    scriptId: callFrames[0].location.scriptId,
  });
  await dp.Runtime.getProperties({
    objectId: callFrames[0].scopeChain[0].object.objectId,
    ownProperties: false,
    accessorPropertiesOnly: false,
    nonIndexedPropertiesOnly: false,
    generatePreview: true,
  });

  testRunner.log('Call stack:');
  callFrames.forEach((frame, index) => {
    testRunner.log(`    ${index}) ${frame.functionName} (${frame.url}:${
        frame.location.lineNumber + 1})`);
  });

  await dp.Debugger.setBreakpointsActive({active: true});
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume({terminateOnResume: false});
  await resumedPromise;
  testRunner.log('Script execution resumed.');

  testRunner.completeTest();
});
