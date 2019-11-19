(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(`Tests that the worker's name is exposed on its Execution Context.\n`);

  await session.evaluate(`
    worker = new Worker('${testRunner.url('../resources/worker-console-worker.js')}', {
      name: 'the name'
    });
  `);
  let workerCallback;
  const workerPromise = new Promise(x => workerCallback = x);
  dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                           flatten: true});
  const {params: {sessionId, targetInfo}} = await dp.Target.onceAttachedToTarget();
  testRunner.log(`target title: "${targetInfo.title}"`);
  const childSession = session.createChild(sessionId);
  childSession.protocol.Runtime.enable({});
  const event = await childSession.protocol.Runtime.onceExecutionContextCreated();
  testRunner.log(`execution context name: "${event.params.context.name}"`);
  testRunner.completeTest();
})
