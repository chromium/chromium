(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() attaches to new page targets.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const response = await target.attachToBrowserTarget();

  const newBrowserSession =
      new TestRunner.Session(testRunner, response.result.sessionId);
  const newUrl = testRunner.url('../resources/test-page.html?newpage');
  newBrowserSession.protocol.Target.createTarget({url: newUrl});
  const attachedEvent = await target.onceAttachedToTarget();
  testRunner.log(attachedEvent, 'Attached to the new page: ');

  const newSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  newSession.protocol.Inspector.onTargetReloadedAfterCrash(
      event => testRunner.log(event, 'FAIL: received spurious event '));
  await newSession.protocol.Runtime.runIfWaitingForDebugger();
  testRunner.log('Resumed\n\n');
  const path = await newSession.protocol.Runtime.evaluate({expression: 'location.href'});
  testRunner.log(path, 'New page location: ');

  await newBrowserSession.disconnect();
  testRunner.completeTest();
})
