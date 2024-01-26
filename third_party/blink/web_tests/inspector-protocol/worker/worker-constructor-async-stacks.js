(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Async stack trace in worker constructor.');
  const debuggers = new Map();

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  testRunner.log('Setup page session');
  const pageDebuggerId = (await dp.Debugger.enable()).result.debuggerId;
  debuggers.set(pageDebuggerId, dp.Debugger);
  await dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  testRunner.log('Set breakpoint before worker created');
  await dp.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 2, columnNumber: 13});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  session.evaluate(`
var blob = new Blob(['console.log(239);//# sourceURL=worker.js'], {type: 'application/javascript'});
var worker = new Worker(URL.createObjectURL(blob));
//# sourceURL=test.js`);

  testRunner.log('Run stepInto with breakOnAsyncCall flag');
  await dp.Debugger.oncePaused();
  await dp.Debugger.stepInto({breakOnAsyncCall: true});

  testRunner.log('Setup worker session');
  const childSession = session.createChild(
      (await attachedPromise).params.sessionId);

  const workerDebuggerId =
        (await childSession.protocol.Debugger.enable()).debuggerId;
  debuggers.set(workerDebuggerId, childSession.protocol.Debugger);
  await childSession.protocol.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  await childSession.protocol.Runtime.runIfWaitingForDebugger();

  const {callFrames, asyncStackTrace, asyncStackTraceId} =
      (await childSession.protocol.Debugger.oncePaused()).params;
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      workerDebuggerId);
  testRunner.completeTest();
})
