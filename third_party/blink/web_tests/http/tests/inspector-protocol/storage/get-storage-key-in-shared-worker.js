(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Storage.getStorageKey() returns the correct value for a shared worker.');

  const pageOrigin = await session.evaluate('location.origin');
  const expectedStorageKey = pageOrigin + '/';

  const {result: {frameTree}} = await dp.Page.getFrameTree();
  const mainTargetStorageKeyResult =
      await dp.Storage.getStorageKey({frameId: frameTree.frame.id});
  const mainStorageKey = mainTargetStorageKeyResult.result.storageKey;

  const bp = testRunner.browserP();
  await bp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const attachedPromise = bp.Target.onceAttachedToTarget(
      e => e.params.targetInfo.type === 'shared_worker');
  session.evaluate(`
    new SharedWorker('${testRunner.url('../resources/empty.js')}')
  `);
  const attachedEvent = await attachedPromise;
  const workerDP = session.createChild(attachedEvent.params.sessionId).protocol;
  const workerStorageKeyResult = await workerDP.Storage.getStorageKey();
  const workerStorageKey = workerStorageKeyResult.result.storageKey;

  testRunner.log(
      `Main storage key is correct: ${mainStorageKey === expectedStorageKey}`);
  testRunner.log(`Worker storage key matches main key: ${
      workerStorageKey === mainStorageKey}`);

  testRunner.completeTest();
})
