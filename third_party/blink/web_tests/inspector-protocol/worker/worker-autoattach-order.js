(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          'Target.setAutoAttach should report all existing workers before returning.');

  // Create two workers and wait until they are initialized. We wait until we
  // can receive onmessage events although this might not be totally necessary.
  await session.evaluateAsync(`
    const w1 = new Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    const promise1 = new Promise(x => w1.onmessage = x);
    const w2 = new Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    const promise2 = new Promise(x => w2.onmessage = x);
    Promise.all([promise1, promise2]);
  `);

  dp.Target.onAttachedToTarget((event) => {
    testRunner.log(event.params.targetInfo.type);
  });

  // setAutoAttach should fire AttachedToTarget events for all existing workers
  // before resolving. We check this in the ordering of the log output.
  await dp.Target.setAutoAttach({
    autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  testRunner.log('setAutoAttach resolved');
  testRunner.completeTest();
})
