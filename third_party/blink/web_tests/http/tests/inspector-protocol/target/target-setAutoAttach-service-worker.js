(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startURL(
    '/inspector-protocol/resources/empty.html',
    `Tests that browser.Target.setAutoAttach() attaches to new service workers.`);
  const swHelper = (await testRunner.loadScript('../service-worker/resources/service-worker-helper.js'))(dp, session);

  const target = testRunner.browserP().Target;
  await Promise.all([
    target.setDiscoverTargets({discover: true}),
    target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true}),
  ]);
  const swTargetPromises = [
    target.onceTargetCreated(),
    target.onceAttachedToTarget(),
  ];
  const activatedPromise = swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/blank-service-worker.js');
  const [swTarget, swAttachedEvent] = await Promise.all(swTargetPromises);
  const swTargetInfo = swTarget.params.targetInfo;
  testRunner.log(`Started and attached to ${swTargetInfo.type} target, waitingForDebugger=${swAttachedEvent.params.waitingForDebugger}`);
  const swSession = new TestRunner.Session(testRunner, swAttachedEvent.params.sessionId);
  testRunner.log('self.globalVar = ' + await swSession.evaluate('self.globalVar'));
  await Promise.all([
    swSession.protocol.Runtime.runIfWaitingForDebugger(),
    activatedPromise,
  ]);
  testRunner.log('Resumed, self.globalVar = ' + await swSession.evaluate('self.globalVar'));
  testRunner.completeTest();
})
