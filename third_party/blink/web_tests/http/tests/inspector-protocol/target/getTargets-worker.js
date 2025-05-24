(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that getTargets returns worker targets.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  await session.evaluateAsync(`window.worker = new Worker('/inspector-protocol/fetch/resources/worker.js');`);

  const {result} = await dp.Target.getTargets();

  testRunner.log(result.targetInfos.filter(info => info.type === 'worker'));

  testRunner.completeTest();
})
