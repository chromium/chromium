(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
    'http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/empty.html', 'Tests target attach and crash for shared storage worklet.');

  await session.evaluateAsync(`
      sharedStorage.worklet.addModule('http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/module.js');
  `);

  const bp = testRunner.browserP();
  bp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});

  const worklet = await bp.Target.onceAttachedToTarget(event => event.params.targetInfo.type === 'shared_storage_worklet');
  testRunner.log(worklet.params.targetInfo);

  const workletSession = session.createChild(worklet.params.sessionId);

  await Promise.all([
    workletSession.protocol.Inspector.onceTargetCrashed(),
    session.evaluate(`
      sharedStorage.run('empty-operation', {keepAlive: false});
    `),
  ]);
  testRunner.log('Stopped shared storage worklet and received Inspector.targetCrashed event\n');

  testRunner.completeTest();
});
