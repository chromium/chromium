(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'http://127.0.0.1:8000/inspector-protocol/resources/empty.html',
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

  try {
    const result = await session.evaluateAsync(`
      (async function() {
        try {
          const registration = await navigator.serviceWorker.register(
              '/inspector-protocol/resources/background-fetch-service-worker.js');
          window.swRegistration = registration;

          let sw = registration.installing || registration.waiting || registration.active;
          if (sw.state !== 'activated') {
            await new Promise(resolve => {
              const onStateChange = () => {
                if (sw.state === 'activated') {
                  sw.removeEventListener('statechange', onStateChange);
                  resolve();
                }
              };
              sw.addEventListener('statechange', onStateChange);
            });
          }

          if (registration.backgroundFetch) {
            const bgFetchReg = await registration.backgroundFetch.fetch(
                'my-fetch', '/inspector-protocol/resources/empty.html');
            if (bgFetchReg.result === '') {
              await new Promise(resolve => {
                const onProgress = () => {
                  if (bgFetchReg.result !== '') {
                    bgFetchReg.removeEventListener('progress', onProgress);
                    resolve();
                  }
                };
                bgFetchReg.addEventListener('progress', onProgress);
              });
            }
            return "fetch " + bgFetchReg.result;
          }
          return "no backgroundFetch API";
        } catch (e) {
          return "error: " + e.message;
        }
      })()
    `);

    testRunner.log('JS result: ' + result);

    testRunner.log('Storage after background fetch');
    const resultUsageAndQuota = await dp.Storage.getUsageAndQuota({origin});
    const after = resultUsageAndQuota.result;
    const breakdown = Object.fromEntries(
        after.usageBreakdown.map(item => [item.storageType, item.usage]));
    const totalBreakdown =
        after.usageBreakdown.reduce((sum, item) => sum + item.usage, 0);
    testRunner.log(`quota: ${after.quota}`);
    testRunner.log(`overrideActive: ${after.overrideActive}`);
    testRunner.log(`file_systems usage == 0: ${breakdown.file_systems === 0}`);
    testRunner.log(`indexeddb usage == 0: ${breakdown.indexeddb === 0}`);
    testRunner.log(`cache_storage usage > 0: ${breakdown.cache_storage > 0}`);
    testRunner.log(
        `service_workers usage > 0: ${breakdown.service_workers > 0}`);
    testRunner.log(
        `total usage matches breakdown: ${after.usage === totalBreakdown}`);
  } finally {
    // Release backgroundfetchsuccess waitUntil and clean up before teardown.
    await session.evaluateAsync(`
      (async function() {
        const registration = window.swRegistration;
        if (registration) {
          if (registration.active) {
            await new Promise(resolve => {
              const channel = new MessageChannel();
              channel.port1.onmessage = resolve;
              registration.active.postMessage('release', [channel.port2]);
            });
          }
          await registration.unregister();
        }
      })()
    `);
    await dp.Storage.clearDataForOrigin({origin, storageTypes: 'all'});
  }

  testRunner.completeTest();
})
