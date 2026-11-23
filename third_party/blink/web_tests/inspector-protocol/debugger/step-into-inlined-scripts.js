// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL(
      'resources/debugger-step-into-inlined-scripts.html',
      'Tests that debugger StepInto will step through inlined scripts.');

  const scripts = new Map();
  dp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  await dp.Page.enable();
  await dp.Debugger.enable();

  const url =
      testRunner.url('resources/debugger-step-into-inlined-scripts.html');

  function formatCallFrames(callFrames) {
    const lines = ['Call stack:'];
    for (let i = 0; i < callFrames.length; ++i) {
      const frame = callFrames[i];
      const frameUrl = scripts.get(frame.location.scriptId) || frame.url;
      const fileName =
          frameUrl ? frameUrl.substring(frameUrl.lastIndexOf('/') + 1) : '';
      lines.push(`    ${i}) ${frame.functionName} (${fileName}:${
          frame.location.lineNumber + 1})`);
    }
    return lines.join('\n');
  }

  const bpResult = await dp.Debugger.setBreakpointByUrl({
    url,
    lineNumber: 6,
  });
  const breakpointId = bpResult.result.breakpointId;
  if (!breakpointId || bpResult.result.locations.length === 0) {
    testRunner.fail(
        `Failed to resolve breakpoint: ${JSON.stringify(bpResult)}`);
    return;
  }

  let pausedPromise = dp.Debugger.oncePaused();
  const loadEventPromise = dp.Page.onceLoadEventFired();
  const reloadPromise = dp.Page.reload();

  const initialPause = await pausedPromise;
  if (!initialPause.params.hitBreakpoints ||
      !initialPause.params.hitBreakpoints.includes(breakpointId)) {
    testRunner.fail(`Expected breakpoint ${breakpointId}, got ${
        JSON.stringify(initialPause.params.hitBreakpoints)}`);
    return;
  }
  testRunner.log(formatCallFrames(initialPause.params.callFrames));
  testRunner.log('');

  const pausedScriptIds =
      new Set([initialPause.params.callFrames[0].location.scriptId]);
  const numberOfStepInto = 6;
  for (let i = 0; i < numberOfStepInto; ++i) {
    testRunner.log('Executing StepInto...');
    pausedPromise = dp.Debugger.oncePaused();
    const resumedPromise = dp.Debugger.onceResumed();
    await dp.Debugger.stepInto();
    await resumedPromise;
    const {params: {callFrames}} = await pausedPromise;
    pausedScriptIds.add(callFrames[0].location.scriptId);
    testRunner.log(formatCallFrames(callFrames));
    testRunner.log('');
  }

  if (pausedScriptIds.size !== 2) {
    testRunner.fail(`Expected to pause in 2 distinct inline scripts, got ${
        pausedScriptIds.size}`);
    return;
  }

  testRunner.log('Executing Resume...');
  const resumedPromise = dp.Debugger.onceResumed();
  await dp.Debugger.resume();
  await Promise.all([resumedPromise, reloadPromise, loadEventPromise]);
  testRunner.log('Page reloaded.');

  testRunner.completeTest();
});
