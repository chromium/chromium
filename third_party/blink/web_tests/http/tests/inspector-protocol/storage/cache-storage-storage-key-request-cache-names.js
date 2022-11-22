(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that requesting cache names for storage key works correctly\n`);
  await dp.Page.enable();

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({
                       frameId: frameId
                     })).result.storageKey;
  await dp.Storage.trackCacheStorageForStorageKey({storageKey});

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

  testRunner.log('\nRequest cache names for storage key');
  const requestCacheNamesPromise = dp.CacheStorage.requestCacheNames({storageKey});
  const caches = (await requestCacheNamesPromise).result.caches;
  testRunner.log(`${caches.length} cache with name: ${caches[0].cacheName}`);

  testRunner.completeTest();
})
