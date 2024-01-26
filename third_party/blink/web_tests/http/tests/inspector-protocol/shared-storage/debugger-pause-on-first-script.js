(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    'http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/empty.html',
    'Tests debugger pause in shared storage worklet.');

  const bp = testRunner.browserP();
  await bp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: true});

  session.evaluateAsync(`
    sharedStorage.worklet.addModule('http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/module.js');
  `);

  const worklet = (await bp.Target.onceAttachedToTarget()).params;
  testRunner.log(worklet);

  const worklet_session = session.createChild(worklet.sessionId);
  const wp = worklet_session.protocol;
  wp.Runtime.onConsoleAPICalled(event => {
    testRunner.log(`[WORKLET]: ${event.params.args[0].value}`);
  });
  const scripts = new Map();
  wp.Debugger.onScriptParsed(({params}) => {
    scripts.set(params.scriptId, params.url);
  });

  wp.Runtime.enable();
  wp.Debugger.enable();
  wp.Debugger.pause();
  wp.Runtime.runIfWaitingForDebugger();

  const paused = (await wp.Debugger.oncePaused()).params;

  const function_location = paused.callFrames[0].functionLocation;
  const location = paused.callFrames[0].location;
  testRunner.log(`Paused at ${scripts.get(function_location.scriptId) ?? '<unknown>'}:${function_location.lineNumber}:${function_location.columnNumber} (function location)`);
  testRunner.log(`Paused at ${scripts.get(location.scriptId) ?? '<unknown>'}:${location.lineNumber}:${location.columnNumber} (location)`);
  worklet_session.evaluate(`testToken = 42;`);
  wp.Debugger.resume();

  // This should return once top-level script completes and hence the
  // console message is logged.
  await worklet_session.evaluate("");
  testRunner.completeTest();
});
