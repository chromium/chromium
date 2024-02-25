(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  async function testBucket(bucketName) {
    const bucket = bucketName === undefined ?
      'window' :
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

    testRunner.log(`Open database, object store for ${bucketName ?? 'default'} bucket`);

    // Create database, objectStore, ans try to access the latter.
    const value = await session.evaluateAsync(`
      new Promise(async resolve => {
        const request = ${bucket}.indexedDB.open("test-database");
        request.onerror = (event) => {
          resolve('failed');
        };
        request.onupgradeneeded = (event) => {
          const db = event.target.result;
          const objectStore = db.createObjectStore("test-store");
          objectStore.add("test-data", "test-key");
          resolve('database and objectstore created');
        }
      })
    `);

    testRunner.log(`${value}\n`);
    testRunner.log(`Get metadata for storage key`);

    const storageBucket = await bucketPromise;
    const getMetadataResult = (await dp.IndexedDB.getMetadata({
      storageBucket,
      databaseName: 'test-database',
      objectStoreName: 'test-store'
    })).result;

    testRunner.log(`metadata equals ${JSON.stringify(getMetadataResult)}\n`);

    // Clean up
    await dp.IndexedDB.deleteDatabase(
      { storageBucket, databaseName: 'test-database' });
  }
  const { dp, session } = await testRunner.startBlank(
    `Tests that getting metadata works for IndexedDB with storage bucket\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
