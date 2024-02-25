(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  async function testBucket(bucketName) {
    const bucket = bucketName === undefined ?
      'window' :
      `(await navigator.storageBuckets.open('${bucketName}'))`;
    const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
    const storageKey =
      (await dp.Storage.getStorageKeyForFrame({ frameId })).result.storageKey;
    const bucketPromise = (async () => {
      dp.Storage.setStorageBucketTracking({ storageKey, enable: true });
      const { params: { bucketInfo: { bucket } } } =
        await dp.Storage.onceStorageBucketCreatedOrUpdated(
          e => e.params.bucketInfo.bucket.name === bucketName);
      return bucket;
    })();

    testRunner.log(`Open database, some object stores and add a value for ${bucketName ?? 'default'} bucket`);

    // Create database, some objectStores and add a value.
    const value = await session.evaluateAsync(`
    new Promise(async resolve => {
      const request = ${bucket}.indexedDB.open("test-database");
      request.onerror = (event) => {
        resolve('failed');
      };
      request.onupgradeneeded = (event) => {
        const db = event.target.result;
        const objectStore = db.createObjectStore("test-store");
        db.createObjectStore("another-test-store");
        objectStore.add("test-data", "test-key");
        resolve('database, object stores created and value added');
      }
    })
  `);

    testRunner.log(`${value}\n`);
    testRunner.log(`Request database for storage key`);

    const storageBucket = await bucketPromise;
    const requestDatabaseResult =
      (await dp.IndexedDB.requestDatabase(
        { storageBucket, databaseName: 'test-database' }))
        .result.databaseWithObjectStores.objectStores.map(
          entry => entry.name);

    testRunner.log(requestDatabaseResult, 'database object store names');

    // Clean up
    await dp.IndexedDB.deleteDatabase(
      { storageBucket, databaseName: 'test-database' });
  }
  const { dp, session } = await testRunner.startBlank(
    `Tests that requesting database works for IndexedDB with storage bucket\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
