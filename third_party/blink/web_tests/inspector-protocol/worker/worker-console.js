(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests how console messages from worker get into page's console once worker is destroyed.`);

  // TODO(johannes): Within this test, it appears that it's necessary to
  // wait until messages have trickled into the page log
  // via dp.Log.onEntryAdded or into the console API
  // via childSession.protocol.Runtime.onConsoleAPICalled, before calling
  // worker.terminate, or otherwise the messages may be dropped, or even
  // appear much later, after another worker has been started.
  // For now, this test is non-flaky primarily by sprinkling several
  // Promise.all calls around postToWorker. Further investigation /
  // clarification / research may be needed.

  await session.evaluate(`
    let worker;

    // This function returns a promise, which, if awaited (e.g. with
    // await session.evaluateAsync("startWorker()") ensures that the
    // worker has been created. This works by worker-console-worker.js posting
    // a message upon which worker.onmessage resolves the promise.
    function startWorker() {
      return new Promise(resolve => {
        worker = new Worker(
            '${testRunner.url('../resources/worker-console-worker.js')}');
        worker.onmessage = resolve;
      });
    }

    // Similar to the above method, this method returns a promise such that
    // awaiting it ensures that the worker has posted a message back to the
    // page. In this way, we know that worker-console-worker.js gets a chance
    // to call console.log.
    // We can terminate the worker after awaiting the promise returned by this
    // method, and then observe that the console.log events were registered
    // anyway. This is much of the point of this test.
    function postToWorker(message) {
      return new Promise(resolve => {
        worker.onmessage = resolve;
        worker.postMessage(message);
      });
    }
  `);

  // To avoid flaky test output, we emit messages into these three different
  // logs, and flush them after each section of the test.
  const clientLog = [];
  const pageLog = [];
  const consoleLog = [];
  function flushLogs() {
    for (let line of clientLog) {
      testRunner.log(line);
    }
    clientLog.length = 0;
    for (let line of pageLog) {
      testRunner.log('<- Log from page: ' + line);
    }
    pageLog.length = 0;
    for (let line of consoleLog) {
      testRunner.log('<-- Console API from worker: ' + line);
    }
    consoleLog.length = 0;
  }

  await dp.Log.enable();
  dp.Log.onEntryAdded((event) => { pageLog.push(event.params.entry.text); });

  function postToWorker(message) {
    clientLog.push('-> Posting to worker: ' + message);
    return session.evaluateAsync('postToWorker("' + message + '")');
  }

  {
    clientLog.push(
        "\n=== console.log event won't get lost despite worker.terminate. ===");
    clientLog.push('Starting worker');
    await session.evaluateAsync('startWorker()');
    await Promise.all([
      postToWorker('message0 (posted after starting worker)'),
      dp.Log.onceEntryAdded()]);
    // The key part of this test is that its expectation contains
    // "<- Log from page: message0 (posted after starting worker)", even
    // though we terminated the worker without awaiting the log event
    // (postToWorker only ensures that the message was echoed back to the page).
    clientLog.push('Terminating worker');
    await session.evaluate('worker.terminate()');
    flushLogs();
  }

  {
    clientLog.push(
        '\n=== Scenario with autoattach enabled and stopped. ===');
    clientLog.push('Starting worker');
    await session.evaluateAsync('startWorker()');
    await Promise.all([postToWorker('message1'), dp.Log.onceEntryAdded()]);

    // Now we call Target.setAutoAttach, which will immediately generate
    // an event that we're attached; which we receive below to create the
    // childSession instance.
    clientLog.push('Starting autoattach');
    const attachedPromise = dp.Target.onceAttachedToTarget();
    await dp.Target.setAutoAttach({
      autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
    const childSession = session.createChild(
        (await attachedPromise).params.sessionId);
    childSession.protocol.Runtime.onConsoleAPICalled((event) => {
      consoleLog.push(event.params.args[0].value);
    });
    clientLog.push('child session for worker created');

    clientLog.push('Sending Runtime.enable to worker');
    childSession.protocol.Runtime.enable();
    await childSession.protocol.Runtime.onceConsoleAPICalled();

    await Promise.all([
      postToWorker('message2 (posted after runtime enabled)'),
      dp.Log.onceEntryAdded(),
      childSession.protocol.Runtime.onceConsoleAPICalled()]);
    await Promise.all([
      postToWorker('throw1 (posted after runtime enabled; ' +
                   'yields exception in worker)'),
      dp.Log.onceEntryAdded()]);

    // This unregisters the child session, so we stop getting the console API
    // calls, but still receive the log messages for the page.
    clientLog.push('Stopping autoattach');
    await dp.Target.setAutoAttach({autoAttach: false,
                                   waitForDebuggerOnStart: false});
    await Promise.all([
      postToWorker('message3 (posted after auto-attach)'),
      dp.Log.onceEntryAdded()]);
    clientLog.push('Terminating worker');
    flushLogs();
    await session.evaluate('worker.terminate()');
  }

  {
    clientLog.push(
        '\n=== Scenario with autoattach from the get-go. ===');
    // This time we start the worker only after Target.setAutoAttach, so
    // we may await the autoattach response.
    clientLog.push('Starting autoattach');
    const attachedPromise = dp.Target.onceAttachedToTarget();
    await dp.Target.setAutoAttach({
      autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

    clientLog.push('Starting worker');
    session.evaluate('startWorker()');
    const childSession = session.createChild(
        (await attachedPromise).params.sessionId);
    childSession.protocol.Runtime.onConsoleAPICalled((event) => {
      consoleLog.push(event.params.args[0].value);
    });
    clientLog.push('child session for worker created');

    // Here, we test the behavior of posting before / after Runtime.enable.
    await Promise.all([
      postToWorker("message4 (posted before worker's runtime agent enabled)"),
      dp.Log.onceEntryAdded()]);

    clientLog.push('Sending Runtime.enable to worker');
    childSession.protocol.Runtime.enable();
    await childSession.protocol.Runtime.onceConsoleAPICalled();
    await Promise.all([
      postToWorker("message5 (posted after worker's runtime agent enabled)"),
      childSession.protocol.Runtime.onceConsoleAPICalled(),
      dp.Log.onceEntryAdded()]);
    clientLog.push('Terminating worker');
    flushLogs();
    await session.evaluate('worker.terminate()');
  }

  {
    clientLog.push(
        '\n=== New worker, with auto-attach still enabled. ===');
    clientLog.push('Starting worker');
    const attachedPromise = dp.Target.onceAttachedToTarget();
    session.evaluate('startWorker()');
    const childSession = session.createChild(
        (await attachedPromise).params.sessionId);
    childSession.protocol.Runtime.onConsoleAPICalled((event) => {
      consoleLog.push(event.params.args[0].value);
    });
    clientLog.push('child session for worker created');

    await Promise.all([
      postToWorker('message6 (posted just before worker termination)'),
      dp.Log.onceEntryAdded()]);

    clientLog.push('Terminating worker');
    clientLog.push('Stopping autoattach');
    flushLogs();
    await session.evaluate('worker.terminate()');

    dp.Target.setAutoAttach({autoAttach: false,
                             waitForDebuggerOnStart: false});
  }
  testRunner.completeTest();
})
