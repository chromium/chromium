(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that requesting data works for IndexedDB with storageKey\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  testRunner.log(`Open database, object store and add some values`);

  // Create database, objectStore, add a key-value pair and read value.
  const value = await session.evaluateAsync(`
    new Promise(async resolve => {
      const request = window.indexedDB.open("test-database");
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

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey =
      (await dp.Storage.getStorageKeyForFrame({frameId})).result.storageKey;
  const requestDataResult =
      (await dp.IndexedDB.requestData({
        storageKey,
        databaseName: 'test-database',
        objectStoreName: 'test-store',
        indexName: '',
        skipCount: 1,
        pageSize: 3
      })).result.objectStoreDataEntries.map(entry => entry.value.value);

  testRunner.log(requestDataResult, 'data key values equal');

  // Clean up
  await dp.IndexedDB.deleteDatabase({storageKey, databaseName: "test-database"});

  testRunner.completeTest();
})
