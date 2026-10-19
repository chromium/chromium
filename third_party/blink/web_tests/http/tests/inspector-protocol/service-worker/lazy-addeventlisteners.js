// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that a Log.entryAdded warning is reported if addEventListener is called after initial evaluation of the service worker script.\n`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const resources = '/inspector-protocol/service-worker/resources/';

  // Test a lifecycle event.
  await registerAndDumpWarning(
      'service-worker-lazy-install-addeventlistener.js', 'lazy-install-scope');
  // Test a functional event.
  await registerAndDumpWarning('service-worker-lazy-fetch-addeventlistener.js',
                               'lazy-fetch-scope');

  testRunner.completeTest();

  async function registerAndDumpWarning(script, scope) {
    const attachedPromise = dp.Target.onceAttachedToTarget(
        event => event.params.targetInfo.url.endsWith(script));
    session.evaluateAsync(
        (script, scope) => navigator.serviceWorker.register(script, {scope}),
        resources + script, resources + scope);
    const attached = await attachedPromise;
    const swSession = session.createChild(attached.params.sessionId);
    const swdp = swSession.protocol;

    const entryPromise = swdp.Log.onceEntryAdded();
    await Promise.all([
      swdp.Log.enable(),
      swdp.Debugger.enable(),
      swdp.Debugger.setAsyncCallStackDepth({maxDepth: 32}),
      swdp.Runtime.runIfWaitingForDebugger(),
    ]);
    const {entry} = (await entryPromise).params;
    testRunner.log(`${entry.source} ${entry.level}: ${entry.text}`);
    for (let stack = entry.stackTrace; stack; stack = stack.parent) {
      if (stack.description) {
        testRunner.log(`    -- ${stack.description} --`);
      }
      for (const frame of stack.callFrames) {
        const name = frame.functionName || '(anonymous)';
        const file = frame.url.substring(frame.url.lastIndexOf('/') + 1);
        testRunner.log(
            `    ${name} @ ${file}:${frame.lineNumber}:${frame.columnNumber}`);
      }
    }
  }
});
