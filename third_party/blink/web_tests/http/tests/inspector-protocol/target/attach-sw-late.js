(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests Target.attachToTarget can attach to service worker later on`);
  const bp = testRunner.browserP();
  const swHelper = (await testRunner.loadScript('../service-worker/resources/service-worker-helper.js'))(dp, session);

  dp.ServiceWorker.enable();
  testRunner.log(`Registering service worker for scope1`);
  await swHelper.installSWAndWaitForActivated('/inspector-protocol/target/resources/mint-service-worker.pl?scope1', {
      scope: '/inspector-protocol/target/resources/scope1/'
  });

  const targets = (await bp.Target.getTargets()).result.targetInfos;
  const swTarget = targets.find(target => target.type === 'service_worker');
  testRunner.log(swTarget);

  dp.Target.attachToTarget({targetId: swTarget.targetId, flatten: true});
  const attachedParams = (await dp.Target.onceAttachedToTarget()).params;
  testRunner.log(attachedParams);

  const swSession = session.createChild(attachedParams.sessionId);
  swSession.protocol.Runtime.runIfWaitingForDebugger();
  testRunner.log(await swSession.protocol.Inspector.onceWorkerScriptLoaded());

  testRunner.completeTest();
})
