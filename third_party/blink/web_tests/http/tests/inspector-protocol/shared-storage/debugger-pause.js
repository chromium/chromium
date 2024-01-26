(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    'http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/empty.html', 'Tests debugger pause in shared storage worklet.');

  await session.evaluateAsync(`
      sharedStorage.worklet.addModule('http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/module.js');
  `);

  const bp = testRunner.browserP();
  bp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});

  const worklet = await bp.Target.onceAttachedToTarget(event => event.params.targetInfo.type === 'shared_storage_worklet');

  const workletSession = session.createChild(worklet.params.sessionId);
  await workletSession.protocol.Debugger.enable();

  session.evaluate(`
    sharedStorage.run(
      'set-global-var-and-pause-on-debugger-operation',
      {
        data: {'setGlobalVarTo': 5},
        keepAlive: true
      }
    );
  `);

  await workletSession.protocol.Debugger.oncePaused();

  testRunner.log('Paused in the worklet');
  testRunner.log('Worklet state: globalVar=' + await workletSession.evaluate(`globalVar`));

  testRunner.completeTest();
});
