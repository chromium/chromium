(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Storage.getStorageKey() returns the correct value for a dedicated worker.');

  const pageOrigin = await session.evaluate('location.origin');

  const expectedStorageKey = pageOrigin + '/';

  const {result: {frameTree}} = await dp.Page.getFrameTree();
  const mainTargetStorageKeyResult =
      await dp.Storage.getStorageKey({frameId: frameTree.frame.id});
  const mainStorageKey = mainTargetStorageKeyResult.result.storageKey;

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget(
      e => e.params.targetInfo.type === 'worker');
  session.evaluate(`
    new Worker('${testRunner.url('../resources/empty.js')}')
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
