(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(`Tests that the worker's name is exposed on its Execution Context.\n`);

  await session.evaluate(`
    worker = new Worker('${testRunner.url('../resources/worker-console-worker.js')}', {
      name: 'the name'
    });
  `);
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                           flatten: true});
  const {params: {sessionId, targetInfo}} = await attachedPromise;
  testRunner.log(`target title: "${targetInfo.title}"`);
  const childSession = session.createChild(sessionId);
  const contextPromise = childSession.protocol.Runtime.onceExecutionContextCreated();
  await childSession.protocol.Runtime.enable({});
  const event = await contextPromise;
  testRunner.log(`execution context name: "${event.params.context.name}"`);
  testRunner.completeTest();
})
