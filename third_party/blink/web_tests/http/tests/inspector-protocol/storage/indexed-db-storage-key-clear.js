(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that clearing object store works for IndexedDB with storageKey\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  testRunner.log(`Open database, object store and set value`);

  // Create database, objectStore, add a key-value pair and read value.
  const value = await session.evaluateAsync(`
    new Promise(async resolve => {
      const request = window.indexedDB.open("test-database");
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

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({frameId: frameId})).result.storageKey;
  await dp.IndexedDB.clearObjectStore({storageKey: storageKey, databaseName: "test-database", objectStoreName: "test-store"});

  // Open database, objectStore and read value.
  const valueAfterClear = await session.evaluateAsync(`
    new Promise(async resolve => {
      const openreq = window.indexedDB.open("test-database");
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

  testRunner.log(`data value after clear equals: ${JSON.stringify(valueAfterClear)}`);

  // Clean up
  await dp.IndexedDB.deleteDatabase({storageKey, databaseName: "test-database"});

  testRunner.completeTest();
})
