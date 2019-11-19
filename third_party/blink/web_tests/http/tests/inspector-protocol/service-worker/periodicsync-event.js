(async testRunner => {
  const { page, session, dp } = await testRunner.startURL(
    'resources/periodicsync-event-worker.html',
    `Tests delivery of a periodicsync event to the service worker.`);

  async function waitForServiceWorkerActivation() {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== "activated");
    return versions[0].registrationId;
  }

  const registrationIdPromise = waitForServiceWorkerActivation();

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();
  const registrationId = await registrationIdPromise;

  dp.ServiceWorker.dispatchPeriodicSyncEvent({ origin: 'http://127.0.0.1:8000', registrationId: registrationId, tag: 'devtools-test-tag' });
  const tag = await session.evaluateAsync('window.__periodicsyncTagPromise');
  testRunner.log(`Got periodicsync event with tag: ` + tag);

  testRunner.completeTest();
});
