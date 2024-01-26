(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() executes script on load for window.open'ed pages.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const promise = session.evaluate(`
    window.open('').foo
  `);
  testRunner.log('Opened popup window');
  const attachedEvent = await target.onceAttachedToTarget();
  testRunner.log('Attached to the popup window, waitingForDebugger=' + attachedEvent.params.waitingForDebugger);

  const popupSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  popupSession.protocol.Runtime.enable();
  popupSession.protocol.Page.addScriptToEvaluateOnNewDocument({ source: 'window.foo = 0;' });
  popupSession.protocol.Page.addScriptToEvaluateOnNewDocument({ source: 'window.foo += 42;' });

  await popupSession.protocol.Runtime.runIfWaitingForDebugger();
  testRunner.log('Resumed popup window');

  testRunner.log('result: ' + await promise);

  testRunner.completeTest();
})
