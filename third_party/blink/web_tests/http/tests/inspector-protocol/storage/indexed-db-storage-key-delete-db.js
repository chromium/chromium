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

    testRunner.log(`Open database for ${bucketName ?? 'default'} bucket`);

    // Create database and objectStore.
    const open = await session.evaluateAsync(`
      new Promise(async resolve => {
        const request = ${bucket}.indexedDB.open("test-database");
        request.onerror = (event) => {
          resolve('failed to create database');
        };
        request.onupgradeneeded = (event) => {
          const db = event.target.result;
          const objectStore = db.createObjectStore("test-store");
          resolve('database created successfuly');
        };
      })
    `);

    testRunner.log(`${open}\n`);
    testRunner.log(`Delete database`);

    const storageBucket = await bucketPromise;
    await dp.IndexedDB.deleteDatabase(
      { storageBucket, databaseName: 'test-database' });

    // Open database, try to access objectStore.
    const accessAfterDelete = await session.evaluateAsync(`
      new Promise(async resolve => {
        const openreq = ${bucket}.indexedDB.open("test-database");
        openreq.onerror = (event) => {
          resolve("not able to open database");
        }
        openreq.onsuccess = (event) => {
          const db = event.target.result;
          try {
            db.transaction(['test-store'],'readwrite').objectStore('test-store');
          } catch (error) {
            resolve("database deleted");
          }
        };
      })
    `);

    testRunner.log(`${accessAfterDelete}\n`);

    // Clean up
    await dp.IndexedDB.deleteDatabase(
        {storageBucket, databaseName: 'test-database'});
  }

  const { dp, session } = await testRunner.startBlank(
    `Tests that deleting database works for IndexedDB with storage bucket\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  await testBucket();
  await testBucket('test-bucket');

  testRunner.completeTest();
})
