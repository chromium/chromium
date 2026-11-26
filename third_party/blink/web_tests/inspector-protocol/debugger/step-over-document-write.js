// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that debugger StepOver will skip inlined scripts created by document.write().\n');

  await dp.Page.enable();
  await dp.Runtime.enable();
  await dp.Debugger.enable();

  dp.Runtime.onConsoleAPICalled(({params}) => {
    if (params.type === 'assert' || params.type === 'error') {
      testRunner.fail(`Unexpected console.${params.type}: ${
          params.args.map(arg => arg.value ?? arg.description).join(' ')}`);
    }
  });
  dp.Runtime.onExceptionThrown(({params}) => {
    testRunner.fail(`Unexpected exception: ${params.exceptionDetails.text}`);
  });

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  const targetUrl = testRunner.url('resources/step-over-document-write.html');
  await session.navigate(targetUrl);

  const bp = await dp.Debugger.setBreakpointByUrl({
    url: targetUrl,
    lineNumber: 3,
    columnNumber: 0,
  });
  if (!bp.result.breakpointId || bp.result.locations.length === 0) {
    testRunner.fail(`Failed to set breakpoint: ${JSON.stringify(bp)}`);
    return;
  }
  testRunner.log('Breakpoint set.');

  function printCallFrames(callFrames) {
    testRunner.log('Call stack:');
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const rawUrl = frame.url || scripts.get(frame.location.scriptId) || '';
      const fileName = rawUrl ? rawUrl.split('/').pop() : '';
      testRunner.log(`    ${i}) ${frame.functionName} (${fileName}:${
          frame.location.lineNumber + 1})`);
    }
    testRunner.log('');
  }

  const numberOfStepOver = 3;
  let pausedPromise = dp.Debugger.oncePaused();
  const loadEventPromise = dp.Page.onceLoadEventFired();
  const reloadPromise = dp.Page.reload();

  const firstPause = await pausedPromise;
  if (!firstPause.params.hitBreakpoints ||
      !firstPause.params.hitBreakpoints.includes(bp.result.breakpointId)) {
    testRunner.fail(
        `Expected pause on breakpoint ${bp.result.breakpointId}, got ${
            JSON.stringify(firstPause.params.hitBreakpoints)}`);
    return;
  }
  printCallFrames(firstPause.params.callFrames);

  for (let i = 0; i < numberOfStepOver; ++i) {
    testRunner.log('Executing StepOver...');
    const resumedPromise = dp.Debugger.onceResumed();
    pausedPromise = dp.Debugger.oncePaused();
    await dp.Debugger.stepOver();
    await resumedPromise;
    const pause = await pausedPromise;
    printCallFrames(pause.params.callFrames);
  }

  testRunner.log('Executing Resume...');
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await resumedPromise;
  await reloadPromise;
  await loadEventPromise;
  testRunner.log('Page reloaded.');

  const fooResult = await dp.Runtime.evaluate({expression: 'window.foo'});
  testRunner.log(`window.foo=${fooResult.result.result.value}`);

  testRunner.completeTest();
});
