(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() attaches to window.open targets.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.evaluate(`
    window.myWindow = window.open('../resources/inspector-protocol-page.html'); undefined;
  `);
  testRunner.log('Opened the window');
  const attachedEvent = await target.onceAttachedToTarget();
  testRunner.log('Attached to window, waitingForDebugger=' + attachedEvent.params.waitingForDebugger);
  const popupSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  const changedPromise = target.onceTargetInfoChanged();
  await popupSession.protocol.Runtime.runIfWaitingForDebugger();
  testRunner.log('Resumed popup window');
  const changeEvent = await changedPromise;
  testRunner.log('Popup window URL changed to ' + changeEvent.params.targetInfo.url);

  const secondChangedPromise = target.onceTargetInfoChanged();
  session.evaluate(`
    window.myWindow.location.assign('../resources/test-page.html'); undefined;
  `);
  testRunner.log('Navigated the window');
  const secondChangeEvent = await secondChangedPromise;
  testRunner.log('Target info changed, new URL is ' + secondChangeEvent.params.targetInfo.url);

  const detachedPromise = target.onceDetachedFromTarget();
  session.evaluate(`
    window.myWindow.close(); undefined;
  `);
  testRunner.log('Closed the window');
  await detachedPromise;
  testRunner.log('Detached from window');

  testRunner.completeTest();
})
