// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that a Log.entryAdded warning is reported if addEventListener adds an empty fetch handler.\n`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'service_worker');
  session.evaluateAsync(`navigator.serviceWorker.register(
          '/inspector-protocol/service-worker/resources/service-worker-empty-fetch-handler.js',
          {scope: '/inspector-protocol/service-worker/resources/empty-fetch-handler-scope'})`);
  const attached = await attachedPromise;
  const swSession = session.createChild(attached.params.sessionId);
  const swdp = swSession.protocol;

  // Expecting that a warning message on adding a no-op fetch handler is
  // reported by the service worker target.
  const entryPromise = swdp.Log.onceEntryAdded();
  await Promise.all([
    swdp.Log.enable(),
    swdp.Runtime.runIfWaitingForDebugger(),
  ]);
  const {entry} = (await entryPromise).params;
  testRunner.log(`${entry.source} ${entry.level}: ${entry.text}`);

  testRunner.completeTest();
});
