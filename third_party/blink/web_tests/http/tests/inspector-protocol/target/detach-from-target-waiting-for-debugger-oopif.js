(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that an out-of-process iframe that waits for the debugger is resumed when the session is detached from it.`);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget();
  const loadedPromise = session.evaluateAsync(`
    new Promise(resolve => {
      const iframe = document.createElement('iframe');
      iframe.src = 'http://devtools.oopif.test:8000/inspector-protocol/resources/test-page.html';
      iframe.onload = () => resolve('Iframe loaded');
      document.body.appendChild(iframe);
    })
  `);
  const {sessionId, targetInfo, waitingForDebugger} =
      (await attachedPromise).params;
  testRunner.log(`Attached to ${targetInfo.type}, waitingForDebugger=${
      waitingForDebugger}`);

  // Do not send Runtime.runIfWaitingForDebugger.
  await dp.Target.detachFromTarget({sessionId});
  testRunner.log('Detached');

  testRunner.log(await loadedPromise);
  testRunner.completeTest();
})
