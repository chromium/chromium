(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/inspector-protocol/resources/empty.html',
      `Tests that browser.Target.setAutoAttach() only throttles service worker for sessions that asked for it.`);
  const swHelper = (await testRunner.loadScript('../service-worker/resources/service-worker-helper.js'))(dp, session);

  const attachedToPage = (await testRunner.browserP().Target.attachToTarget({targetId: page.targetId(), flatten: true})).result;
  const session2 = new TestRunner.Session(testRunner, attachedToPage.sessionId);
  const dp2 = session2.protocol;

  dp.Target.onAttachedToTarget(event => {
    testRunner.log(`session 1 attached, waiting: ${event.params.waitingForDebugger}`);
    const swSession = new TestRunner.Session(testRunner, event.params.sessionId);
    swSession.protocol.Runtime.runIfWaitingForDebugger();
  })
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  await dp2.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const workerReady = swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/blank-service-worker.js');

  const attachedToSW = (await dp2.Target.onceAttachedToTarget(event => event.params.targetInfo.type === "service_worker")).params;
  testRunner.log(`session 2 attached, waiting: ${attachedToSW.waitingForDebugger}`);
  const swSession = new TestRunner.Session(testRunner, attachedToSW.sessionId);
  swSession.protocol.Runtime.runIfWaitingForDebugger();
  await workerReady;
  const href = await swSession.evaluate(`location.href`);
  testRunner.log(`PASSED: service worker URL ${href}`);
  testRunner.completeTest();
})
