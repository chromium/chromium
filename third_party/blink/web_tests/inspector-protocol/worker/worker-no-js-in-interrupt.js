(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Runtime.enable and Runtime.getExceptionDetails do not ' +
      'interrupt a running worker loop, while Debugger.pause and ' +
      'Runtime.terminateExecution do.');

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
    const workerScript = \`
      let state = 'idle';
      let arr = [1, 2, 3, 4];
      let enableBuffer = new ArrayBuffer(64);
      let enableTa = new Uint8Array(enableBuffer);
      let shouldEnableTransfer = false;
      Error.prepareStackTrace = function() {
        arr.length = 1;
        return '\\\\n    at prepared:' + state;
      };
      class EnableTransferError extends Error {
        get name() {
          if (shouldEnableTransfer && enableBuffer.byteLength > 0) {
            enableBuffer.transfer(0);
          }
          return 'EnableTransferError:' + state;
        }
      }
      console.log(new Error('logged-error'));
      console.log(new EnableTransferError('logged-transfer'));

      let buffer = new ArrayBuffer(16);
      let ta = new Uint8Array(buffer);
      ta[0] = 7;
      let shouldTransfer = false;
      class TransferError extends Error {
        get name() {
          if (shouldTransfer && buffer.byteLength > 0) {
            buffer.transfer(0);
          }
          return 'TransferError:' + state;
        }
      }
      self.transferErr = new TransferError('boom');

      // Each busy-loop handler spins in while (true) until the test ends the
      // turn via Runtime.terminateExecution.
      self.onmessage = function(e) {
        if (e.data === 'busy-loop-enable') {
          shouldEnableTransfer = true;
          state = 'in-loop';
          postMessage('loop-started');
          let sum = 0;
          while (true) {
            arr[3] = (sum + 1) & 0xff;
            enableTa[0] = (sum + 2) & 0xff;
            sum = (sum + arr[3] + enableTa[0]) | 0;
          }
        } else if (e.data === 'busy-loop-details') {
          shouldTransfer = true;
          state = 'in-ta-loop';
          postMessage('ta-loop-started');
          let sum = 0;
          while (true) {
            ta[0] = (sum + 1) & 0xff;
            sum = (sum + ta[0]) | 0;
          }
        } else if (e.data === 'infinite-loop') {
          postMessage('infinite-started');
          while (true) {}
        }
      };
    \`;
    const blob = new Blob([workerScript], {type: 'text/javascript'});
    window.worker = new Worker(URL.createObjectURL(blob));
    const messageQueue = [];
    const messageResolvers = [];
    window.worker.onmessage = e => {
      if (messageResolvers.length > 0) {
        messageResolvers.shift()(e.data);
      } else {
        messageQueue.push(e.data);
      }
    };
    window.waitForWorkerMessage = function() {
      if (messageQueue.length > 0) {
        return Promise.resolve(messageQueue.shift());
      }
      return new Promise(resolve => messageResolvers.push(resolve));
    };
  `);

  const {params: {sessionId}} = await attachedPromise;
  const workerSession = session.createChild(sessionId);

  const enableDebuggerRes = await workerSession.protocol.Debugger.enable();
  testRunner.log(
      'Debugger.enable error: ' + (enableDebuggerRes.error ?? 'none'));

  // 1. Runtime.enable must not interrupt the running loop (covers both
  // Error.prepareStackTrace shrinking an array and Error name getter
  // transferring an ArrayBuffer).
  let startedPromise = session.evaluateAsync('window.waitForWorkerMessage()');
  await session.evaluate(`window.worker.postMessage('busy-loop-enable')`);
  testRunner.log('Worker status: ' + await startedPromise);

  const consoleMessages = [];
  workerSession.protocol.Runtime.onConsoleAPICalled(e => {
    consoleMessages.push(e.params.args[0].description);
  });
  // Queue Runtime.enable (non-interrupting), then send an allow-listed
  // Debugger.setBreakpointsActive command on the same IO session pipe. When
  // setBreakpointsActive responds, Runtime.enable has already been received by
  // the worker's IOSession while the loop is still running.
  let enableResolved = false;
  const enablePromise = workerSession.protocol.Runtime.enable().then(res => {
    enableResolved = true;
    return res;
  });
  const activeRes1 =
      await workerSession.protocol.Debugger.setBreakpointsActive({active: true});
  testRunner.log('Debugger.setBreakpointsActive during loop 1 error: ' +
                 (activeRes1.error ?? 'none'));
  testRunner.log('Runtime.enable resolved during loop: ' + enableResolved);

  // Terminate the busy loop via the allow-listed Runtime.terminateExecution so
  // the current worker turn ends without entering a nested pause message loop,
  // allowing the queued non-interrupting Runtime.enable task to run next.
  await workerSession.protocol.Runtime.terminateExecution();
  await enablePromise;
  testRunner.log('Console error 1: ' + consoleMessages[0]);
  testRunner.log('Console error 2: ' + consoleMessages[1].split('\n')[0]);

  // 2. Runtime.getExceptionDetails must not interrupt the running loop.
  const {result: {result: errObj}} =
      await workerSession.protocol.Runtime.evaluate({
        expression: 'self.transferErr',
      });
  startedPromise = session.evaluateAsync('window.waitForWorkerMessage()');
  await session.evaluate(`window.worker.postMessage('busy-loop-details')`);
  testRunner.log('Worker status: ' + await startedPromise);

  let detailsResolved = false;
  const detailsPromise = workerSession.protocol.Runtime
      .getExceptionDetails({
        errorObjectId: errObj.objectId,
      })
      .then(res => {
        detailsResolved = true;
        return res;
      });
  const activeRes2 =
      await workerSession.protocol.Debugger.setBreakpointsActive({active: true});
  testRunner.log('Debugger.setBreakpointsActive during loop 2 error: ' +
                 (activeRes2.error ?? 'none'));
  testRunner.log(
      'Runtime.getExceptionDetails resolved during loop: ' + detailsResolved);

  await workerSession.protocol.Runtime.terminateExecution();
  const detailsRes = await detailsPromise;
  testRunner.log(
      'Exception details description: ' +
      detailsRes.result.exceptionDetails.exception.description.split('\n')[0]);

  // 3. Debugger.pause and Runtime.terminateExecution must interrupt a worker
  // stuck in an infinite loop.
  startedPromise = session.evaluateAsync('window.waitForWorkerMessage()');
  await session.evaluate(`window.worker.postMessage('infinite-loop')`);
  testRunner.log('Worker status: ' + await startedPromise);

  const pausePromise = workerSession.protocol.Debugger.pause();
  await workerSession.protocol.Debugger.oncePaused();
  const pauseRes = await pausePromise;
  testRunner.log('Debugger.pause error: ' + (pauseRes.error ?? 'none'));
  const resumeRes = await workerSession.protocol.Debugger.resume();
  testRunner.log('Debugger.resume error: ' + (resumeRes.error ?? 'none'));

  const terminateRes =
      await workerSession.protocol.Runtime.terminateExecution();
  testRunner.log(
      'Runtime.terminateExecution error: ' + (terminateRes.error ?? 'none'));

  testRunner.completeTest();
})
