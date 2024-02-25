(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests sourceURL in setTimeout from worker.');

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false,
                           flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker-string-setTimeout.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  testRunner.log('Started worker');

  const messageObject = await attachedPromise;
  const childSession = session.createChild(messageObject.params.sessionId);

  testRunner.log('Worker created');
  testRunner.log('didConnectToWorker');

  await childSession.protocol.Debugger.enable();
  testRunner.log('Did enable debugger');

  // posting a message to the worker triggers the onmessage function in
  // dedicated-worker-string-setTimeout.js, which calls setTimeout("foo()", 0);
  // - and as a side-effect of that, "foo()" is getting parsed as it has to be
  // evaluated.
  // Whereas parsing the worker script itself would result in a
  // Debugger.scriptParsed message that reports a result url, parsing this
  // string results in a message that has the url field set to the empty string.
  // We use this fact below to verify that indeed, we did evaluate setTimeout
  // in the worker.

  session.evaluate('worker.postMessage(1)');
  testRunner.log('Did post message to worker');

  // Skip the script event.
  await childSession.protocol.Debugger.onceScriptParsed();

  const sourceUrl =
      (await childSession.protocol.Debugger.onceScriptParsed()).params.url;
  if (sourceUrl === '') {
    testRunner.log('SUCCESS: script created from string parameter of ' +
                   'setTimeout has no url');
  } else {
    testRunner.log('FAIL: script created from string parameter of ' +
                   'setTimeout has url ' + sourceUrl);
  }
  testRunner.completeTest();
})
