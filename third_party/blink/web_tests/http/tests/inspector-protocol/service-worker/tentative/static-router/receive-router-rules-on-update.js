(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/service-worker-with-static-router.html',
      'Tests receiving static router rules from the service worker.');

  async function waitForServiceWorkerActivation() {
    let versions = [];
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (versions.length == 0 || versions[0].status != "installed");
    return versions;
  }

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();

  const versions = await waitForServiceWorkerActivation();
  testRunner.log(versions[0].routerRules);
  testRunner.completeTest();
});
