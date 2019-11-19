(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank(
          'Target.setAutoAttach should report all workers before returning.');

  await session.evaluate(`
    const w1 = new Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    const promise1 = new Promise(x => w1.onmessage = x);
    const w2 = new Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    const promise2 = new Promise(x => w2.onmessage = x);
    Promise.all([promise1, promise2]);
  `);

  let autoAttachPromiseResolved = false;

  const autoAttachPromise = dp.Target.setAutoAttach({
    autoAttach: true, waitForDebuggerOnStart: false, flatten: true}).then(
        () => { autoAttachPromiseResolved = true; });

  testRunner.log((await dp.Target.onceAttachedToTarget()).params.targetInfo.type);
  testRunner.log((await dp.Target.onceAttachedToTarget()).params.targetInfo.type);

  // Up to here, the promise from Target.setAutoAttach is still not resolved,
  // meaning that we've received the attachedToTarget events before
  // setAutoAttach has returned. We log this fact, and then show that our
  // mechanism for testing (the autoAttachPromiseResolved variable) is
  // working by awaiting and logging again.
  testRunner.log('Before await. Resolved: ' + autoAttachPromiseResolved);
  await autoAttachPromise;
  testRunner.log('After await. Resolved: ' + autoAttachPromiseResolved);
  testRunner.completeTest();
})
