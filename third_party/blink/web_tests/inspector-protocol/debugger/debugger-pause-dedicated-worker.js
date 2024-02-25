(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that worker can be paused.');

  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  testRunner.log('Started worker');

  const attachedPromise = dp.Target.onceAttachedToTarget();
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                           flatten: true});

  const event = await attachedPromise;
  const childSession = session.createChild(event.params.sessionId);
  testRunner.log('Worker created');
  testRunner.log('didConnectToWorker');

  await childSession.protocol.Debugger.enable({});
  childSession.protocol.Debugger.pause({});
  await childSession.protocol.Debugger.oncePaused();
  testRunner.log('SUCCESS: Worker paused');

  await childSession.protocol.Debugger.disable({});
  testRunner.completeTest();
})
