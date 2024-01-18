(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() attaches to window.open targets.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.evaluate(`
    window.myWindow = window.open('/inspector-protocol/emulation/resources/echo-headers.php');
    undefined;
  `);
  testRunner.log('Opened the window');
  const attachedEvent = await target.onceAttachedToTarget();
  testRunner.log('Attached to window, waitingForDebugger=' + attachedEvent.params.waitingForDebugger);
  const popupSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  popupSession.protocol.Page.enable();
  await popupSession.protocol.Emulation.setUserAgentOverride({userAgent: 'Lynx v0.1'});
  popupSession.protocol.Runtime.runIfWaitingForDebugger();
  await popupSession.protocol.Page.onceLoadEventFired();
  const textContent = await popupSession.evaluate(`document.body.textContent`);
  const userAgent = textContent.split('\n').find(line => /^User-Agent:/.test(line));
  testRunner.log(`got: ${userAgent}`);
  testRunner.completeTest();
})
