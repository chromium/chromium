(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests auto-attach of shared workers.');

  await session.evaluateAsync(`
      new Promise(resolve => {
        window.worker = new SharedWorker('/inspector-protocol/fetch/resources/shared-worker.js');
        worker.port.onmessage = event => {
          if (event.data === 'ready')
            resolve();
        };
      })
  `);

  const bp = testRunner.browserP();
  bp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});
  const worker1 = await bp.Target.onceAttachedToTarget(event => event.params.targetInfo.type === 'shared_worker');
  testRunner.log(worker1.params.targetInfo);

  session.evaluate(`
     window.worker2 = new SharedWorker('/inspector-protocol/fetch/resources/shared-worker.js?worker2');
  `);
  const worker2 = await bp.Target.onceAttachedToTarget(event => event.params.targetInfo.type === 'shared_worker');
  testRunner.log(worker2.params.targetInfo);

  testRunner.completeTest();
});
