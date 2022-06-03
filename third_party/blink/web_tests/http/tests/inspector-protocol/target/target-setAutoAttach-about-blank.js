(async function(testRunner) {
  await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() attaches to new about:blank page.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const response = await target.attachToBrowserTarget();

  const newBrowserSession = new TestRunner.Session(testRunner, response.result.sessionId);
  newBrowserSession.protocol.Target.createTarget({url: 'about:blank#newpage'});
  testRunner.log('Created new page from another session');

  const attachedEvent = await target.onceAttachedToTarget();
  testRunner.log(attachedEvent, 'Auto-attached to the new page: ');

  // Navigate elsewhere and test that the request will be paused.
  const newSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  const logSpuriousEvent = event => testRunner.log(event, 'FAIL: received spurious event while paused ');
  target.onTargetInfoChanged(logSpuriousEvent);
  newSession.navigate(testRunner.url('../resources/test-page.html?newpage'));
  // Do a roundtrip to the browser to wait for some time when
  // TargetInfoChanged could come.
  await target.getTargets();
  target.offTargetInfoChanged(logSpuriousEvent);
  const [infoChangedEvent] = await Promise.all([
    target.onceTargetInfoChanged(),
    newSession.protocol.Runtime.runIfWaitingForDebugger()
  ]);
  testRunner.log('Resumed');
  testRunner.log(infoChangedEvent, 'Received new target info: ');

  await newBrowserSession.disconnect();
  testRunner.completeTest();
})
