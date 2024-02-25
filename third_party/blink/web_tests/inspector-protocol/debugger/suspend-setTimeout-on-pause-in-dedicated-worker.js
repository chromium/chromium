(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Tests that setTimeout callback will not fire while script execution is paused. Bug 377926.');

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true,
                           flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker-suspend-setTimeout.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  testRunner.log('Started worker');

  const event = await attachedPromise;
  const childSession = session.createChild(event.params.sessionId);
  testRunner.log('Worker created');

  await childSession.protocol.Debugger.enable();
  await childSession.protocol.Runtime.runIfWaitingForDebugger();
  await childSession.protocol.Debugger.oncePaused();
  testRunner.log('SUCCESS: Worker paused');
  const value = await childSession.evaluate('global_value');
  if (value === 1) {
    // If the setTimeout in dedicated-worker-suspend-setTimeout.js
    // had fired, global_value === 2014.
    testRunner.log('SUCCESS: global_value is 1');
  }
  await childSession.protocol.Debugger.disable();
  testRunner.completeTest();
})
