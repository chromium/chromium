(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Async stack trace for worker.onmessage.');
  const debuggers = new Map();

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  testRunner.log('Setup page session');
  const pageDebuggerId = (await dp.Debugger.enable()).result.debuggerId;
  debuggers.set(pageDebuggerId, dp.Debugger);
  dp.Debugger.setAsyncCallStackDepth({maxDepth: 32});
  session.evaluate(`
var blob = new Blob(['postMessage(239);//# sourceURL=worker.js'], {type: 'application/javascript'});
var worker = new Worker(URL.createObjectURL(blob));
worker.onmessage = (e) => console.log(e.data);
//# sourceURL=test.js`);

  testRunner.log('Setup worker session');
  const childSession = session.createChild(
      (await dp.Target.onceAttachedToTarget()).params.sessionId);
  const workerDebuggerId =
        (await childSession.protocol.Debugger.enable()).result.debuggerId;
  debuggers.set(workerDebuggerId, childSession.protocol.Debugger);
  childSession.protocol.Debugger.setAsyncCallStackDepth({maxDepth: 32});

  testRunner.log('Set breakpoint before postMessage');
  childSession.protocol.Debugger.setBreakpointByUrl(
      {url: 'worker.js', lineNumber: 0, columnNumber: 0});

  testRunner.log('Run worker');
  childSession.protocol.Runtime.runIfWaitingForDebugger();

  testRunner.log('Run stepInto with breakOnAsyncCall flag');
  await childSession.protocol.Debugger.oncePaused();

  childSession.protocol.Debugger.stepInto({breakOnAsyncCall: true});

  const {callFrames, asyncStackTraceId} = (await dp.Debugger.oncePaused()).params;
  await testRunner.logStackTrace(
      debuggers, {callFrames, parentId: asyncStackTraceId}, pageDebuggerId);
  testRunner.completeTest();
})
