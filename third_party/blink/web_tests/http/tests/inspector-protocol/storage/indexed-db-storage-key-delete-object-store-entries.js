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
      `Open database, object store and set values for ${bucketName ?? 'default'} bucket`);

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

    testRunner.log(value, 'data values');
    testRunner.log(
      `Delete object store entries with keys greater than 'test-key1'`);

    const storageBucket = await bucketPromise;
    const keyRange = {
      lower: { type: 'string', string: 'test-key2' },
      lowerOpen: false,
      upperOpen: true
    };
    await dp.IndexedDB.deleteObjectStoreEntries({
      storageBucket,
      databaseName: 'test-database',
      objectStoreName: 'test-store',
      keyRange
    });

    // Open database, objectStore and read values.
    const valueAfterClear = await session.evaluateAsync(`
      new Promise(async resolve => {
        const openreq = ${bucket}.indexedDB.open("test-database");
        openreq.onerror = (event) => {
          resolve("not able to open database");
        }
        openreq.onsuccess = (event) => {
          const db = event.target.result;
          const store = db.transaction(['test-store'],'readwrite').objectStore('test-store');
          const result = [];
          for (let i = 0; i < 5; i++) {
            const getReq = store.get("test-key" + i);
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

    testRunner.log(valueAfterClear, 'data values after clear');

    // Clean up
    await dp.IndexedDB.deleteDatabase(
      { storageBucket, databaseName: 'test-database' });
  }

  const { dp, session } = await testRunner.startBlank(
    `Tests that deleting object store entries works for IndexedDB with storage bucket\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
