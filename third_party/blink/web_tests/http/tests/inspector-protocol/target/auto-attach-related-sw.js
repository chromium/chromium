(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests Target.autoAttachRelated attachers to new SW version when watchin a SW target`);
  const bp = testRunner.browserP();
  const swHelper = (await testRunner.loadScript('../service-worker/resources/service-worker-helper.js'))(dp, session);

  dp.ServiceWorker.enable();

  testRunner.log(`Registering service worker for scope1`);
  await swHelper.installSWAndWaitForActivated('/inspector-protocol/target/resources/mint-service-worker.pl?scope1', {
      scope: '/inspector-protocol/target/resources/scope1/'
  });

  const targets = (await bp.Target.getTargets()).result.targetInfos;
  const swTarget1 = targets.find(target => target.type === 'service_worker');
  bp.Target.onAttachedToTarget(event => {
    const targetInfo = event.params.targetInfo;
    testRunner.log(`Attached to target ${targetInfo.url} (${targetInfo.type}), waiting: ${
        event.params.waitingForDebugger}`);
  });

  testRunner.log(`Registering service worker for scope2`);
  await swHelper.installSWAndWaitForActivated('/inspector-protocol/target/resources/mint-service-worker.pl?scope2', {
      scope: '/inspector-protocol/target/resources/scope2/'});

  testRunner.log(`Enabling auto-attach`);
  await bp.Target.autoAttachRelated({targetId: swTarget1.targetId, waitForDebuggerOnStart: true});

  testRunner.log(`Updating service workers`);
  // Update all registrations, but expect only one auto-attach for scope1.
  session.evaluateAsync(`(async function() {
    const registrations = await navigator.serviceWorker.getRegistrations();
    for (const reg of registrations)
      reg.update();
  })()`);

  function waitWorkerUpdated(url_re) {
    return dp.ServiceWorker.onceWorkerVersionUpdated(e =>
         e.params.versions.find(v => url_re.test(v.scriptURL) &&
                                v.runningStatus === 'starting'));
  }
  const allVersionsUpdated = Promise.all([waitWorkerUpdated(/scope1$/), waitWorkerUpdated(/scope2$/)]);
  const attachedToNewVersion = (await bp.Target.onceAttachedToTarget()).params;
  const session2 = new TestRunner.Session(testRunner, attachedToNewVersion.sessionId);
  const token1 = await session2.evaluate(`self.testToken = 41`);
  await session2.protocol.Runtime.runIfWaitingForDebugger();
  await dp.ServiceWorker.onceWorkerVersionUpdated(e => e.params.versions.find(v => /scope1$/.test(v.scriptURL) && v.runningStatus === 'running'));
  const token = await session2.evaluate('self.testToken');
  testRunner.log(`Token in the new version: ${token}`);

  // Assure the worker for another scope is not attached/paused.
  await allVersionsUpdated;
  testRunner.completeTest();
})
