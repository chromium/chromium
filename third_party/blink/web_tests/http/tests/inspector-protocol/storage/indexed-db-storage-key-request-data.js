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

    testRunner.log(`Open database, object store and add some values for ${bucketName ?? 'default'} bucket`);

    // Create database, objectStore, add a key-value pair and read value.
    const value = await session.evaluateAsync(`
      new Promise(async resolve => {
        const request = ${bucket}.indexedDB.open("test-database");
        request.onerror = (event) => {
          resolve('failed to create database');
        };
        request.onupgradeneeded = (event) => {
          const db = event.target.result;
          const objectStore = db.createObjectStore("test-store");
          let result = [];
          for (let i = 0; i < 5; i++) {
            objectStore.add("test-data" + i, "test-key" + i);
            const getReq = objectStore.get("test-key" + i);
            getReq.onsuccess = (event) => {
              result.push(getReq.result);
              if (result.length === 5) {
                resolve(result);
              }
            };
          }
        };
      })
    `);

    testRunner.log(value, 'values added');
    testRunner.log(`\nRequest data for storage key`);

    const storageBucket = await bucketPromise;
    const requestDataResult =
      (await dp.IndexedDB.requestData({
        storageBucket,
        databaseName: 'test-database',
        objectStoreName: 'test-store',
        indexName: '',
        skipCount: 1,
        pageSize: 3
      })).result.objectStoreDataEntries.map(entry => entry.value.value);

    testRunner.log(requestDataResult, 'data key values equal');

    // Clean up
    await dp.IndexedDB.deleteDatabase(
      { storageBucket, databaseName: 'test-database' });
  }

  const { dp, session } = await testRunner.startBlank(
    `Tests that requesting data works for IndexedDB with storage bucket\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
