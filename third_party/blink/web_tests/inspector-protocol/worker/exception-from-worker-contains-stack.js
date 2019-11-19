(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that console message from worker contains stack trace.');

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                                 flatten: true});

  session.evaluate(`
    window.worker1 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
    window.worker1.onerror = function(e) {
      e.preventDefault();
      worker1.terminate();
    }
  `);
  let event = await dp.Target.onceAttachedToTarget();
  const childSession = session.createChild(event.params.sessionId);
  testRunner.log('Worker created');
  await childSession.protocol.Runtime.enable();
  session.evaluate('worker1.postMessage(239);');
  await dp.Target.onceDetachedFromTarget();
  testRunner.log('Worker destroyed');

  session.evaluate(`
    window.worker2 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
  `);
  event = await dp.Target.onceAttachedToTarget();
  const childSession2 = session.createChild(event.params.sessionId);
  testRunner.log('\nWorker created');
  await childSession2.protocol.Runtime.enable();

  session.evaluate('worker2.postMessage(42);');
  event = await childSession2.protocol.Runtime.onceExceptionThrown();
  const callFrames = event.params.exceptionDetails.stackTrace ? event.params.exceptionDetails.stackTrace.callFrames : [];
  testRunner.log(callFrames.length > 0 ? 'Message with stack trace received.' : '[FAIL] Message contains empty stack trace');

  testRunner.completeTest();
})
