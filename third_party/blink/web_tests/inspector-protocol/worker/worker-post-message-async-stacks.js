(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Async stack trace for worker.postMessage.');
  const debuggers = new Map();

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  testRunner.log('Setup page session');
  const pageDebuggerId = (await dp.Debugger.enable()).result.debuggerId;
  debuggers.set(pageDebuggerId, dp.Debugger);
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  testRunner.log('Set breakpoint before postMessage');
  dp.Debugger.setBreakpointByUrl(
      {url: 'test.js', lineNumber: 3, columnNumber: 7});
  session.evaluate(`
var blob = new Blob(['onmessage = (e) => console.log(e.data);//# sourceURL=worker.js'], {type: 'application/javascript'});
var worker = new Worker(URL.createObjectURL(blob));
worker.postMessage(42);
//# sourceURL=test.js`);

  testRunner.log('Run stepInto with breakOnAsyncCall flag');
  await dp.Debugger.oncePaused();
  dp.Debugger.stepInto({breakOnAsyncCall: true});

  testRunner.log('Setup worker session');
  const childSession = session.createChild(
      (await dp.Target.onceAttachedToTarget()).params.sessionId);

  const workerDebuggerId =
        (await childSession.protocol.Debugger.enable()).result.debuggerId;
  debuggers.set(workerDebuggerId, childSession.protocol.Debugger);
  childSession.protocol.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  testRunner.log('Run worker');
  childSession.protocol.Runtime.runIfWaitingForDebugger();
  const {callFrames, asyncStackTrace, asyncStackTraceId} =
      (await childSession.protocol.Debugger.oncePaused()).params;
  await testRunner.logStackTrace(
      debuggers,
      {callFrames, parent: asyncStackTrace, parentId: asyncStackTraceId},
      workerDebuggerId);
  testRunner.completeTest();
})
