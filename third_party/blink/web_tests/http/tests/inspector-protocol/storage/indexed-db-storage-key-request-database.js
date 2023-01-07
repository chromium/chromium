(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that requesting database works for IndexedDB with storageKey\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  testRunner.log(`Open database, some object stores and add a value`);

  // Create database, some objectStores and add a value.
  const value = await session.evaluateAsync(`
    new Promise(async resolve => {
      const request = window.indexedDB.open("test-database");
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

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey =
      (await dp.Storage.getStorageKeyForFrame({frameId})).result.storageKey;
  const requestDatabaseResult =
      (await dp.IndexedDB.requestDatabase({
        storageKey,
        databaseName: 'test-database'
      })).result.databaseWithObjectStores.objectStores.map(entry => entry.name);

  testRunner.log(requestDatabaseResult, 'database object store names');

  // Clean up
  await dp.IndexedDB.deleteDatabase({storageKey, databaseName: "test-database"});

  testRunner.completeTest();
})
