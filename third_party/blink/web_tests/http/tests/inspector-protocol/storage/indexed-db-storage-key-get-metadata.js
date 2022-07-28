(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that getting metadata works for IndexedDB with storageKey\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  testRunner.log(`Open database, object store`);

  // Create database, objectStore, ans try to access the latter.
  const value = await session.evaluateAsync(`
    new Promise(async resolve => {
      const request = window.indexedDB.open("test-database");
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

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({frameId: frameId})).result.storageKey;
  const getMetadataResult = (await dp.IndexedDB.getMetadata({storageKey, databaseName: "test-database", objectStoreName: "test-store"})).result;

  testRunner.log(`metadata equals ${JSON.stringify(getMetadataResult)}`);

  // Clean up
  await dp.IndexedDB.deleteDatabase({storageKey, databaseName: "test-database"});

  testRunner.completeTest();
})
