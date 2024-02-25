(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.oopif-a.test:8443/inspector-protocol/resources/coi-with-iframe.php',
      `Tests that the attachedToTarget message is dispatched to all attached sessions`);

  async function onAttached(sessionName, attachedEvent) {
    testRunner.log(`${sessionName} attached, waitingForDebugger=${
        attachedEvent.params.waitingForDebugger}`);
    const popupSession =
        new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
    await popupSession.protocol.Runtime.runIfWaitingForDebugger();
  }

  await dp.Target.enable();
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const session1Attached =
      dp.Target.onceAttachedToTarget().then(onAttached.bind(null, 'Session 1'));

  const dp2 = (await page.createSession()).protocol;
  await dp2.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const session2Attached = dp2.Target.onceAttachedToTarget().then(
      onAttached.bind(null, 'Session 2'));

  // This will force the iframe to navigate out-of-process.
  await session.evaluate(
      'document.getElementById(\'iframe\').src=\'https://devtools.oopif-b.test:8443/inspector-protocol/resources/corp-iframe.php\'');

  await Promise.all([session1Attached, session2Attached]);
  testRunner.completeTest();
})
