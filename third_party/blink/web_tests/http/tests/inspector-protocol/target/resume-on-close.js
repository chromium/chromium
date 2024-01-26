(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that closing the page will resume the render process.`);

  await dp.Target.setDiscoverTargets({discover: true});
  session.evaluate(`
    window.myWindow = window.open('../resources/inspector-protocol-page.html'); undefined;
  `);
  testRunner.log('Opened a second window');

  var event = await dp.Target.onceTargetInfoChanged(event => event.params.targetInfo.url.endsWith('inspector-protocol-page.html'));
  var targetId = event.params.targetInfo.targetId;

  var sessionId = (await dp.Target.attachToTarget({targetId: targetId, flatten: true})).result.sessionId;
  const childSession = session.createChild(sessionId);
  testRunner.log('Attached to a second window');
  await childSession.protocol.Debugger.enable();
  childSession.evaluate('debugger;');
  await childSession.protocol.Debugger.oncePaused();
  testRunner.log('Paused in a second window');

  session.evaluate(`
    window.myWindow.close(); undefined;
  `);
  testRunner.log('Closed a second window');
  await dp.Target.onceTargetDestroyed(event => event.params.targetId === targetId);
  testRunner.log('Received window destroyed notification');

  await session.evaluateAsync(`
    new Promise(f => setTimeout(f, 0))
  `);
  testRunner.log('setTimeout worked in first window');
  testRunner.completeTest();
})
