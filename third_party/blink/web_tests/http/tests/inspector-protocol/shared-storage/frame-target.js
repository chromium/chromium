(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
    'http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/empty.html',
    'Tests auto-attach to frame target.');

  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: true});

  session.evaluateAsync(`
    sharedStorage.worklet.addModule('http://127.0.0.1:8000/inspector-protocol/shared-storage/resources/module.js');
  `);

  const worklet = (await dp.Target.onceAttachedToTarget()).params;
  testRunner.log(worklet);

  const wp = session.createChild(worklet.sessionId).protocol;

  wp.Runtime.enable();
  wp.Debugger.enable();
  wp.EventBreakpoints.setInstrumentationBreakpoint({eventName: 'sharedStorageWorkletScriptFirstStatement'});
  wp.Runtime.runIfWaitingForDebugger();

  const {data, reason} = (await wp.Debugger.oncePaused()).params;

  testRunner.log({data, reason});

  testRunner.completeTest();
});
