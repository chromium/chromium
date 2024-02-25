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

    testRunner.log(`Open some databases for ${bucketName ?? 'default'} bucket`);

    // Create some databases.
    const value = await session.evaluateAsync(`
    new Promise(async resolve => {
      let databaseNames = [];
      for (let i = 0; i < 5; i++) {
        const request = ${bucket}.indexedDB.open("test-database" + i);
        request.onerror = (event) => {
          resolve('failed');
        };
        request.onsuccess = (event) => {
          databaseNames.push("test-database" + i);
          if (databaseNames.length === 5) {
            resolve(databaseNames);
          }
        }
      }
    })
  `);

    testRunner.log(value, 'databases created with following names');
    testRunner.log(`\nRequest database names for storage key`);

    const storageBucket = await bucketPromise;
    const requestDatabaseNamesResult =
      (await dp.IndexedDB.requestDatabaseNames({
        storageBucket,
      })).result;

    testRunner.log(requestDatabaseNamesResult, 'database names');

    // Clean up
    let cleanUpPromises = [];
    for (let i = 0; i < 5; i++) {
      cleanUpPromises.push(dp.IndexedDB.deleteDatabase(
        { storageBucket, databaseName: 'test-database' + i }));
    }
    await Promise.all(cleanUpPromises);
  }
  const { dp, session } = await testRunner.startBlank(
    `Tests that requesting database names works for IndexedDB with storage bucket\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
