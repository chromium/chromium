(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that worker can be interrupted with Debugger.pause.');

  await dp.Target.setAutoAttach({autoAttach: true,
                                 waitForDebuggerOnStart: false,
                                 flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker-loop.js')}');
    var resolve;
    window.workerMessageReceivedPromise = new Promise(f => resolve = f);
    window.worker.onmessage = function(event) {
      if (event.data === 'WorkerMessageReceived')
        resolve();
    };
  `);
  testRunner.log('Started worker');

  const sessionId = (await attachedPromise).params.sessionId;
  testRunner.log('Worker created');
  testRunner.log('didConnectToWorker');

  const childSession = session.createChild(sessionId);

  // Enable debugger so that V8 can interrupt and handle inspector
  // commands while there is a script running in a tight loop.

  await childSession.protocol.Debugger.enable();
  testRunner.log('Did enable debugger');

  // Start tight loop in the worker.
  await session.evaluate('worker.postMessage(1);');
  testRunner.log('Did post message to worker');

  await session.evaluateAsync('workerMessageReceivedPromise');

  await childSession.protocol.Debugger.pause();
  await childSession.protocol.Debugger.oncePaused();
  testRunner.log('Paused in worker');

  const messageId = await childSession.evaluate('message_id');
  if (messageId > 1) {
    testRunner.log('SUCCESS: messageId > 1');
  } else {
    testRunner.log('FAIL: evaluated, messageId: ' + message_id);
  }

  testRunner.completeTest();
})
