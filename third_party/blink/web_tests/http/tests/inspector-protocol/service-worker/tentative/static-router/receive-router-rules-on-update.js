(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, page} = await testRunner.startBlank(
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

  const versionsPromise = waitForServiceWorkerActivation();
  await dp.ServiceWorker.enable();
  await page.navigate('resources/service-worker-with-static-router.html');

  const versions = await versionsPromise;
  testRunner.log(versions[0].routerRules);
  testRunner.completeTest();
});
