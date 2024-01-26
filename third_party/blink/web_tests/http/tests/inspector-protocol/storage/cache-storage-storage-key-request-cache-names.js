(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  async function testBucket(bucketName) {
    const bucket = bucketName === undefined ?
      'self' :
      `(await navigator.storageBuckets.open('${bucketName}'))`;

    const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
    const storageKey = (await dp.Storage.getStorageKeyForFrame({
      frameId: frameId
    })).result.storageKey;
    const bucketPromise = (async () => {
      dp.Storage.setStorageBucketTracking({ storageKey, enable: true });
      const { params: { bucketInfo: { bucket } } } =
        await dp.Storage.onceStorageBucketCreatedOrUpdated(
          e => e.params.bucketInfo.bucket.name === bucketName);
      return bucket;
    })();

    await dp.Storage.trackCacheStorageForStorageKey({ storageKey });

    testRunner.log(`Open cache, add item`);
    // Create cache and add an item.
    const addResult = await session.evaluateAsync(`
      (async function() {
        try {
          const cache = await ${bucket}.caches.open("test-cache");
          await cache.add('/inspector-protocol/resources/empty.html');
          return 'cache item added successfully';
        } catch (err) {
          return err;
        }
      })()`);

    testRunner.log(addResult);

    const storageBucket = await bucketPromise;

    testRunner.log(`\nRequest cache names for '${bucketName ?? 'default'}' storage bucket`);
    const requestCacheNamesPromise =
      dp.CacheStorage.requestCacheNames({ storageBucket });
    const caches = (await requestCacheNamesPromise).result.caches;
    testRunner.log(
      `${caches.length} cache with name: ${caches[0].cacheName}\n`);
  }

  const { dp, session } = await testRunner.startBlank(
    `Tests that requesting cache names for storage buckets works correctly\n`);
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
