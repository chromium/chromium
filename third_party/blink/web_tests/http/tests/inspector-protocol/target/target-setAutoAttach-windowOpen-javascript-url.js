(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that browser.Target.setAutoAttach() attaches early to window.open with javascript: urls.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.evaluate(`
    window.open('javascript:window.globalVar = 2020; console.log(123);'); undefined;
  `);
  testRunner.log('Opened popup window');
  const attachedEvent = await target.onceAttachedToTarget();
  testRunner.log('Attached to the popup window, waitingForDebugger=' + attachedEvent.params.waitingForDebugger);
  const popupSession = new TestRunner.Session(testRunner, attachedEvent.params.sessionId);
  popupSession.protocol.Console.enable();

  let globalVar = await popupSession.evaluate(`window.globalVar`);
  testRunner.log('globalVar = ' + globalVar);

  await popupSession.protocol.Runtime.runIfWaitingForDebugger();
  testRunner.log('Resumed popup window');

  const message = await popupSession.protocol.Console.onceMessageAdded();
  testRunner.log(message, 'Received console message in the popup: ');
  globalVar = await popupSession.evaluate(`window.globalVar`);
  testRunner.log('globalVar = ' + globalVar);

  testRunner.completeTest();
})
