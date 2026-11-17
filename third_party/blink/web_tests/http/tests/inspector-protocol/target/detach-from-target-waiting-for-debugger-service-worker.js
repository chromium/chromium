(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      '/inspector-protocol/resources/empty.html',
      `Tests that a service worker that waits for the debugger is resumed when the session is detached from it.`);
  const swHelper = (await testRunner.loadScript(
      '../service-worker/resources/service-worker-helper.js'))(dp, session);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  const activatedPromise = swHelper.installSWAndWaitForActivated(
      '/inspector-protocol/service-worker/resources/blank-service-worker.js');
  const {sessionId, targetInfo, waitingForDebugger} =
      (await attachedPromise).params;
  testRunner.log(`Attached to ${targetInfo.type}, waitingForDebugger=${
      waitingForDebugger}`);

  // Do not send Runtime.runIfWaitingForDebugger.
  await dp.Target.detachFromTarget({sessionId});
  testRunner.log('Detached');

  await activatedPromise;
  testRunner.log('Service worker activated');
  testRunner.completeTest();
})
