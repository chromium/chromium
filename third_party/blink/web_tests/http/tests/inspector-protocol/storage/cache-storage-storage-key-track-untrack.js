(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that tracking and untracking CacheStorage for storage key works\n`);
  await dp.Page.enable();

  // Remove the test cache to prevent leaking from other tests.
  await session.evaluateAsync('caches.delete("test-cache")');

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({
                       frameId: frameId
                     })).result.storageKey;
  await dp.Storage.trackCacheStorageForStorageKey({storageKey});
  const listUpdatedPromise = dp.Storage.onceCacheStorageListUpdated();
  const contentUpdatedPromise = dp.Storage.onceCacheStorageContentUpdated();

  testRunner.log(`Open cache, add item`);

  // Create cache and add an item.
  const valuePromise = session.evaluateAsync(`
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

  testRunner.log(`cache storage list updated for storageKey: ${!!(await listUpdatedPromise).params.storageKey}`);
  testRunner.log(await valuePromise);

  testRunner.log(`\nDelete item from cache`);

  // Delete cache item.
  const deletePromise = session.evaluateAsync(`
    new Promise(async resolve => {
      try {
        const cache = await caches.open("test-cache");
        await cache.delete('/inspector-protocol/resources/empty.html');
        resolve('cache item deleted successfully');
      } catch (err) {
        resolve(err);
      }
    })
  `);

  const contentUpdated = await contentUpdatedPromise;
  testRunner.log(`cache storage content updated for: ${contentUpdated.params.cacheName} for storageKey: ${!!contentUpdated.params.storageKey}`);
  testRunner.log(await deletePromise);

  await dp.Storage.untrackCacheStorageForStorageKey({storageKey});
  dp.Storage.onCacheStorageContentUpdated(message => {testRunner.log(message.params.storageKey)});
  dp.Storage.onCacheStorageListUpdated(message => {testRunner.log(message.params.storageKey)});

  testRunner.log('\nAdd one more item');

  // Add one more item.
  const addAfterUntrackPromise = await session.evaluateAsync(`
    new Promise(async resolve => {
      try {
        const cache = await caches.open("test-cache");
        await cache.add('/inspector-protocol/resources/empty.html');
        resolve('one more cache item added successfully');
      } catch (err) {
        resolve(err);
      }
    })
  `);

  testRunner.log(addAfterUntrackPromise);

  testRunner.completeTest();
})
