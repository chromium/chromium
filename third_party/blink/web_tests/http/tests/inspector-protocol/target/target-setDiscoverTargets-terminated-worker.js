(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      `Tests that browser.Target.setDiscoverTargets() does not report terminated workers.`);
  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});

  session.evaluate((url) => {
    const myWorker = new Worker(url);
    myWorker.postMessage('Hello');
    myWorker.terminate();
  }, testRunner.url('./resources/empty.js'));

  const workerTarget = await target.onceTargetCreated(event => event.params.targetInfo.type === 'worker');
  await target.onceTargetDestroyed(event => event.params.targetId === workerTarget.params.targetInfo.targetId);
  await target.setDiscoverTargets({discover: false});

  const targets = [];
  target.onTargetCreated((event) => event.params.targetInfo.type === 'worker'
    && targets.push(event.params));
  await target.setDiscoverTargets({discover: true});
  testRunner.log(targets);
  testRunner.completeTest();
})
