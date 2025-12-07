(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startBlank(
      `Tests shared storage worklet operation finished events.`);

  if (typeof self.sharedStorage === 'undefined') {
    testRunner.completeTest();
    return;
  }

  const baseOrigin = 'http://127.0.0.1:8000/';
  const resources = baseOrigin + 'inspector-protocol/shared-storage/resources/'

  const events = [];
  await dp.Storage.setSharedStorageTracking({enable: true});

  session.evaluate(`
      const scriptURL = "${resources}shared-storage-module.js";
      sharedStorage.worklet.addModule(scriptURL)
            .then(() => sharedStorage.run("set-operation", {keepAlive: true}));
  `);

  events.push(
      (await dp.Storage.onceSharedStorageWorkletOperationExecutionFinished())
          .params);

  await session.evaluateAsync(`
      sharedStorage.selectURL(
          "test-url-selection-operation",
          [{url: "https://google.com/"}, {url: "https://chromium.org/"}]
      );
  `);

  events.push(
      (await dp.Storage.onceSharedStorageWorkletOperationExecutionFinished())
          .params);

  testRunner.log(events, 'Events: ', [
    'finishedTime', 'executionTime', 'mainFrameId', 'workletTargetId'
  ]);

  testRunner.completeTest();
})
