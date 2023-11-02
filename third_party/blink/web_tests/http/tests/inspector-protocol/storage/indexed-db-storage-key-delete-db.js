(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that deleting database works for IndexedDB with storageKey\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  testRunner.log(`Open database`);

  // Create database and objectStore.
  const open = await session.evaluateAsync(`
    new Promise(async resolve => {
      const request = window.indexedDB.open("test-database");
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

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({frameId: frameId})).result.storageKey;
  await dp.IndexedDB.deleteDatabase({storageKey: storageKey, databaseName: "test-database"});

  // Open database, try to access objectStore.
  const accessAfterDelete = await session.evaluateAsync(`
    new Promise(async resolve => {
      const openreq = window.indexedDB.open("test-database");
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

  testRunner.log(accessAfterDelete);

  testRunner.completeTest();
})
