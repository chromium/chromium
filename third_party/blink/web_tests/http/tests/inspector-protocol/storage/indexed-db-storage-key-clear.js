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

    testRunner.log(
      `Open database, object store and set value for ${bucketName ?? 'default'} bucket`);

    // Create database, objectStore, add a key-value pair and read value.
    const value = await session.evaluateAsync(`
      new Promise(async resolve => {
        const request = ${bucket}.indexedDB.open("test-database");
        request.onerror = (event) => {
          resolve('nothing');
        };
        request.onupgradeneeded = (event) => {
          const db = event.target.result;
          const objectStore = db.createObjectStore("test-store");
          objectStore.add("test-data", "test-key");
          const getReq = objectStore.get("test-key");
          getReq.onsuccess = (event) => {
            resolve(getReq.result);
          };
        };
      })
    `);

    testRunner.log(`data value equals: ${JSON.stringify(value)}\n`);
    testRunner.log(`Clear object store for storage key`);

    const storageBucket = await bucketPromise;
    await dp.IndexedDB.clearObjectStore({
      storageBucket,
      databaseName: 'test-database',
      objectStoreName: 'test-store'
    });

    // Open database, objectStore and read value.
    const valueAfterClear = await session.evaluateAsync(`
      new Promise(async resolve => {
        const openreq = ${bucket}.indexedDB.open("test-database");
        openreq.onerror = (event) => {
          resolve("not able to open database");
        }
        openreq.onsuccess = (event) => {
          const db = event.target.result;
          const store = db.transaction(['test-store'],'readwrite').objectStore('test-store');
          const getReqAfterClear = store.get("test-key");
          getReqAfterClear.onsuccess = (event) => {
            resolve(getReqAfterClear.result);
          };
        };
      })
    `);

    testRunner.log(
      `data value after clear equals: ${JSON.stringify(valueAfterClear)}\n`);

    // Clean up
    await dp.IndexedDB.deleteDatabase(
      { storageBucket, databaseName: 'test-database' });
  }

  const { dp, session } = await testRunner.startBlank(
    `Tests that clearing object store works for IndexedDB with storage bucket\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
