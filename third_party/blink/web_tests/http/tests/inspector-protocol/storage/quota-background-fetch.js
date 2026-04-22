(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startURL(
    'http://127.0.0.1:8000/inspector-protocol/service-worker/resources/push-message-service-worker.html',
    `Tests Storage.getUsageAndQuota with background fetch.\n`);

  const origin = "http://127.0.0.1:8000";

  await dp.Browser.grantPermissions({
    origin: origin,
    permissions: ['backgroundFetch'],
  });

  testRunner.log('Initial storage');
  const result1 = await dp.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(result1.result, ["usage", "quota", "overrideActive", "usageBreakdown", "storageType"], 2));

  testRunner.log('Registering service worker and background fetch');

  const result = await session.evaluateAsync(`
    (async function() {
      try {
        const swUrl = 'blank-service-worker.js';
        const registration = await navigator.serviceWorker.register(swUrl);

        let sw = registration.installing || registration.waiting || registration.active;
        if (sw.state !== 'activated') {
          await new Promise(resolve => {
            sw.addEventListener('statechange', () => {
              if (sw.state === 'activated') resolve();
            });
          });
        }

        if (registration.backgroundFetch) {
          const bgFetchReg = await registration.backgroundFetch.fetch('my-fetch', '/inspector-protocol/resources/empty.html');
          return new Promise(resolve => {
             const check = () => {
                if (bgFetchReg.result !== '') {
                   resolve("fetch " + bgFetchReg.result);
                } else {
                   setTimeout(check, 10);
                }
             };
             check();
          });
        }
        return "no backgroundFetch API";
      } catch (e) {
        return "error: " + e.message;
      }
    })()
  `);

  testRunner.log('JS result: ' + result);

  testRunner.log('Storage after background fetch');
  const resultUsageAndQuota = await dp.Storage.getUsageAndQuota({ origin });
  testRunner.log(JSON.stringify(resultUsageAndQuota.result, ["usage", "quota", "overrideActive", "usageBreakdown", "storageType"], 2));

  // Cleanup to prevent race conditions during teardown.
  await session.evaluateAsync(`
    (async function() {
      const registration = await navigator.serviceWorker.ready;
      if (registration.backgroundFetch) {
        const bgFetchReg = await registration.backgroundFetch.get('my-fetch');
        if (bgFetchReg) {
          // Wait for the fetch to either complete or we abort it explicitly
          // so it doesn't crash the browser on teardown.
          if (bgFetchReg.result === '') {
             await bgFetchReg.abort();
          }
        }
      }
      await registration.unregister();
    })()
  `);

  testRunner.completeTest();
})
