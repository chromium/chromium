// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that we can pause in service worker.\n`);

  const scriptURL =
      '/inspector-protocol/service-worker/resources/service-worker-debugger.js';
  const scope = '/inspector-protocol/service-worker/resources/scope-debugger/';

  let resolvePaused;
  const paused = new Promise(r => resolvePaused = r);
  const scriptURLs = new Map();
  dp.Target.onAttachedToTarget(async ({params}) => {
    const swdp = session.createChild(params.sessionId).protocol;
    swdp.Debugger.onScriptParsed(
        ({params}) => scriptURLs.set(params.scriptId, params.url));
    swdp.Debugger.oncePaused().then(({params}) =>
                                        resolvePaused({swdp, params}));
    // Debugger.enable only responds once the worker runs.
    await Promise.all([
      swdp.Debugger.enable(),
      swdp.Runtime.runIfWaitingForDebugger(),
    ]);
  });
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.evaluateAsync(
      (scriptURL, scope) =>
          navigator.serviceWorker.register(scriptURL, {scope}).then(() => {}),
      scriptURL, scope);

  const {swdp, params} = await paused;
  testRunner.log('Script execution paused.');
  testRunner.log('Call stack:');
  params.callFrames.forEach((frame, index) => {
    const url = scriptURLs.get(frame.location.scriptId);
    const fileName = url.substring(url.lastIndexOf('/') + 1);
    testRunner.log(`    ${index}) ${frame.functionName} (${fileName}:${
        frame.location.lineNumber + 1})`);
  });
  await swdp.Debugger.resume();
  testRunner.log('Script execution resumed.');
  testRunner.completeTest();
})
