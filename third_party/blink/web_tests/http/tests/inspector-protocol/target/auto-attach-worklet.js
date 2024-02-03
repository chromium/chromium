(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests auto-attach of worklets.');

  await dp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: true});

  const modulePromise = session.evaluateAsync(`(function() {
      const url = '/inspector-protocol/target/resources/empty-worklet.js';
      const audioContext = new AudioContext();
      return audioContext.audioWorklet.addModule(url);
  })()`);

  const worklet = (await dp.Target.onceAttachedToTarget(event => event.params.targetInfo.type === 'worklet')).params;
  testRunner.log(worklet, "Attached to worklet");
  const ws = session.createChild(worklet.sessionId);
  ws.protocol.Runtime.runIfWaitingForDebugger();
  await modulePromise;
  testRunner.log('SUCCESS: addModule resolved');
  testRunner.completeTest();
});