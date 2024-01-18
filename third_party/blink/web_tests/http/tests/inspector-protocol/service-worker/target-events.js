(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/inspector-protocol/resources/empty.html',
      'Test that target evens are fired for service worker');
  const swHelper = (await testRunner.loadScript('resources/service-worker-helper.js'))(dp, session);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  await dp.Target.setDiscoverTargets({discover: true});

  const swTargetPromises = Promise.all([
    dp.Target.onceTargetCreated(),
    dp.Target.onceAttachedToTarget(),
  ]);
  const serviceWorkerURL = '/inspector-protocol/service-worker/resources/blank-service-worker.js';
  await swHelper.installSWAndWaitForActivated(serviceWorkerURL);
  const [swTarget, swAttachedEvent] = await swTargetPromises;
  const swTargetInfo = swTarget.params.targetInfo;
  testRunner.log(`Started and attached to ${swTargetInfo.type} target`);

  const swdp = session.createChild(swAttachedEvent.params.sessionId).protocol;
  const [registration] = await Promise.all([
    dp.ServiceWorker.onceWorkerRegistrationUpdated(),
    dp.ServiceWorker.enable(),
  ]);
  const scopeURL = registration.params.registrations[0].scopeURL;

  testRunner.log('Received workerRegistrationUpdated with scopeURL = '  + scopeURL);
  await Promise.all([
    swdp.Inspector.onceTargetCrashed(),
    dp.ServiceWorker.stopAllWorkers(),
  ]);
  testRunner.log('Stopped service worker and received Inspector.targetCrashed event\n');

  await Promise.all([
    swdp.Inspector.onceTargetReloadedAfterCrash(),
    dp.ServiceWorker.startWorker({scopeURL}),
  ]);
  testRunner.log('Restarted service worker and received Inspector.targetReloadedAfterCrash event');
  const [swDestroyedEvent] = await Promise.all([
    dp.Target.onceTargetDestroyed(),
    dp.ServiceWorker.unregister({scopeURL}),
    dp.ServiceWorker.stopAllWorkers(),
  ]);
  testRunner.log('Unregistered service worker and received Target.targetDestroyed event for the worker: ' + (swTargetInfo.targetId === swDestroyedEvent.params.targetId));

  testRunner.completeTest();
});
