(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that tasks order is not changed when worker is resumed.');
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true,
                           flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  await session.evaluate(`
      const workerScript = \`
        self.count = 0;
        self.onmessage = msg => {
          if (++self.count === 1) {
            debugger;
            console.log("Should be one:", self.count);
            debugger;
          } else {
            (function FAIL_Should_Not_Pause_Here() { debugger; })();
          }
        };
        //# sourceURL=worker.js\`;
      const blob = new Blob([workerScript], { type: "text/javascript" });
      worker = new Worker(URL.createObjectURL(blob));
      worker.postMessage(1);
    `);

  const event = await attachedPromise;
  const childSession = session.createChild(event.params.sessionId);
  testRunner.log('Worker created');

  await childSession.protocol.Debugger.enable();

  await childSession.protocol.Runtime.runIfWaitingForDebugger();
  await childSession.protocol.Debugger.oncePaused();

  await session.evaluate(`worker.postMessage(2)`);
  await childSession.protocol.Debugger.resume();
  await childSession.protocol.Debugger.oncePaused();
  const value = await childSession.evaluate('self.count');
  testRunner.log(`count must be 1: ${value}`);

  await childSession.protocol.Debugger.disable();
  testRunner.completeTest();
})
