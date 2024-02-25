(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that console message from worker contains stack trace.');

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                                 flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  session.evaluate(`
    window.worker1 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
    window.worker1.onerror = function(e) {
      e.preventDefault();
      worker1.terminate();
    }
  `);
  let event = await attachedPromise;
  const childSession = session.createChild(event.params.sessionId);
  testRunner.log('Worker created');
  await childSession.protocol.Runtime.enable();
  const detachedPromise = dp.Target.onceDetachedFromTarget();
  session.evaluate('worker1.postMessage(239);');
  await detachedPromise;
  testRunner.log('Worker destroyed');

  const attachedPromise2 = dp.Target.onceAttachedToTarget();
  session.evaluate(`
    window.worker2 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
  `);
  event = await attachedPromise2;
  const childSession2 = session.createChild(event.params.sessionId);
  testRunner.log('\nWorker created');
  await childSession2.protocol.Runtime.enable();

  const thrownPromise = childSession2.protocol.Runtime.onceExceptionThrown();
  session.evaluate('worker2.postMessage(42);');
  event = await thrownPromise;
  const callFrames = event.params.exceptionDetails.stackTrace ? event.params.exceptionDetails.stackTrace.callFrames : [];
  testRunner.log(callFrames.length > 0 ? 'Message with stack trace received.' : '[FAIL] Message contains empty stack trace');

  testRunner.completeTest();
})
