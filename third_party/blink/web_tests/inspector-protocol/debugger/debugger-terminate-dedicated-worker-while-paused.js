(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that inspected page won't crash if inspected worker is terminated while it is paused. Test passes if it doesn't crash. Bug 101065.`);

  dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                           flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  testRunner.log('Started worker');
  const sessionId = (await attachedPromise).params.sessionId;
  const childSession = session.createChild(sessionId);
  testRunner.log('Worker created');
  testRunner.log('didConnectToWorker');
  await childSession.protocol.Debugger.enable();
  childSession.protocol.Debugger.pause();
  await childSession.protocol.Debugger.oncePaused();
  testRunner.log('Worker paused');
  await session.evaluate('worker.terminate()');
  testRunner.log('SUCCESS: Did terminate paused worker');
  testRunner.completeTest();
})
