(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session, dp} = await testRunner.startBlank(
      `Tests that getTargetInfo works on a worker target.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const workerAttached = dp.Target.onceAttachedToTarget();
  session.evaluateAsync(`window.worker = new Worker('/inspector-protocol/fetch/resources/worker.js');`);

  const workerSession = session.createChild((await workerAttached).params.sessionId);
  const worker_dp = workerSession.protocol;

  const targetInfo = (await dp.Target.getTargetInfo()).result.targetInfo;
  const workerTargetInfo = (await worker_dp.Target.getTargetInfo()).result.targetInfo;
  const explicitWorkerTargetInfo = (await worker_dp.Target.getTargetInfo({targetId: workerTargetInfo.targetId})).result.targetInfo;
  if (JSON.stringify(workerTargetInfo) !== JSON.stringify(explicitWorkerTargetInfo)) {
    testRunner.log('ERROR: targetInfo mismatch!');
    testRunner.log(workerTargetInfo);
    testRunner.log(explicitWorkerTargetInfo);
  }
  testRunner.log(workerTargetInfo);

  const notAllowedError = (await worker_dp.Target.getTargetInfo({targetId: targetInfo.targetId})).error;
  testRunner.log(notAllowedError);

  testRunner.completeTest();
})
