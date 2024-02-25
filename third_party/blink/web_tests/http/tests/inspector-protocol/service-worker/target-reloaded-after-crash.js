(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      '/inspector-protocol/resources/empty.html',
      'Test that Inspector.targetReloadedAfterCrash is not fired when service worker just created(not restarted)');
  const swHelper = (await testRunner.loadScript('resources/service-worker-helper.js'))(dp, session);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const serviceWorkerURL = '/inspector-protocol/service-worker/resources/blank-service-worker.js';
  const attachedPromise = dp.Target.onceAttachedToTarget();
  swHelper.installSWAndWaitForActivated(serviceWorkerURL);
  const swAttachedEvent = await attachedPromise;
  testRunner.log(`Started and attached to ${swAttachedEvent.params.targetInfo.type} target`);

  const swdp = session.createChild(swAttachedEvent.params.sessionId).protocol;
  swdp.Inspector.onTargetReloadedAfterCrash(() => {
    testRunner.log('FAIL: targetReloadedAfterCrash received for a new service worker')
  });
  await swdp.Inspector.enable();
  const [registration] = await Promise.all([
    dp.ServiceWorker.onceWorkerRegistrationUpdated(),
    dp.ServiceWorker.enable(),
  ]);
  const scopeURL = registration.params.registrations[0].scopeURL;
  await swdp.Runtime.runIfWaitingForDebugger();

  await Promise.all([
    swdp.Inspector.onceTargetCrashed(),
    dp.ServiceWorker.stopAllWorkers(),
  ]);
  testRunner.log('Stopped service worker and received Inspector.targetCrashed event\n');

  await dp.ServiceWorker.unregister({scopeURL});

  testRunner.completeTest();
});
