(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      `Tests that a window.open() target that waits for the debugger is resumed, together with its opener, when the session is detached from it.`);

  const target = testRunner.browserP().Target;
  await target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const attachedPromise = target.onceAttachedToTarget();
  // The opener is paused until the popup is resumed.
  const openedPromise = session.evaluate(`
    window.popup = window.open('../resources/test-page.html');
    'Opener resumed';
  `);
  const {sessionId, targetInfo, waitingForDebugger} =
      (await attachedPromise).params;
  testRunner.log(`Attached to ${targetInfo.type}, waitingForDebugger=${
      waitingForDebugger}`);

  // Do not send Runtime.runIfWaitingForDebugger.
  await target.detachFromTarget({sessionId});
  testRunner.log('Detached');

  testRunner.log(await openedPromise);
  testRunner.log(await session.evaluateAsync(async () => {
    while (!window.popup.document.querySelector('#rootDiv'))
      await new Promise(resolve => setTimeout(resolve, 50));
    return 'Popup loaded';
  }));
  testRunner.completeTest();
})
