(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that dedicated worker won't crash on attempt to step into. Bug 232392.`);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true,
                           flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker-step-into.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  testRunner.log('Started worker');

  const sessionId = (await attachedPromise).params.sessionId;
  testRunner.log('Worker created');

  const childSession = session.createChild(sessionId);

  await childSession.protocol.Debugger.enable();
  await childSession.protocol.Runtime.runIfWaitingForDebugger();

  // onmessage in dedicated-worker-step-into.js will run into the
  // debugger statement, pausing itself. Below, we step into a couple of times
  // while recording the transition into 'doWork' in the worker script.
  await childSession.protocol.Debugger.oncePaused();
  testRunner.log('SUCCESS: Worker paused');

  testRunner.log('Stepping into...');
  await childSession.protocol.Debugger.stepInto();

  const onMessageCallFrame =
        (await childSession.protocol.Debugger.oncePaused()).params.callFrames[0];
  testRunner.log('Paused in ' + onMessageCallFrame.functionName);

  await childSession.protocol.Debugger.stepInto();
  const doWorkCallFrame =
        (await childSession.protocol.Debugger.oncePaused()).params.callFrames[0];
  testRunner.log('Paused in ' + doWorkCallFrame.functionName);

  await childSession.protocol.Debugger.disable();
  testRunner.completeTest();
})
