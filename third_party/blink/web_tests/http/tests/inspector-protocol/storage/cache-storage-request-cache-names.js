(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that requesting cache names for origin works correctly\n`);
  await dp.Page.enable();

  const stabilizeNames = [...TestRunner.stabilizeNames, 'storageKey'];

  const origin = (await dp.Page.getResourceTree()).result.frameTree.frame.securityOrigin;
  await dp.Storage.trackCacheStorageForOrigin({origin});

  testRunner.log(`Open cache, add item`);
  // Create cache and add an item.
  const addPromise = session.evaluateAsync(`
    new Promise(async resolve => {
      try {
        const cache = await caches.open("test-cache");
        await cache.add('/inspector-protocol/resources/empty.html');
        resolve('cache item added successfully');
      } catch (err) {
        resolve(err);
      }
    })
  `);

  testRunner.log(await addPromise);

  testRunner.log('\nRequest cache names for origin');
  const requestCacheNamesPromise = dp.CacheStorage.requestCacheNames({securityOrigin: origin});
  const caches = (await requestCacheNamesPromise).result.caches;
  testRunner.log(caches, '', stabilizeNames);
  testRunner.log(`security origin differs from storage key: ${caches[0].securityOrigin !== caches[0].storageKey}`);

  testRunner.completeTest();
})
