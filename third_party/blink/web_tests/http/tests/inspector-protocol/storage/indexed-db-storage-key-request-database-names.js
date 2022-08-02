(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that requesting database names works for IndexedDB with storageKey\n`);

  await dp.IndexedDB.enable();
  await dp.Page.enable();

  testRunner.log(`Open some databases`);

  // Create some databases.
  const value = await session.evaluateAsync(`
    new Promise(async resolve => {
      let databaseNames = [];
      for (let i = 0; i < 5; i++) {
        const request = window.indexedDB.open("test-database" + i);
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

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey =
      (await dp.Storage.getStorageKeyForFrame({frameId})).result.storageKey;
  const requestDatabaseNamesResult =
      (await dp.IndexedDB.requestDatabaseNames({
        storageKey,
      })).result;

  testRunner.log(requestDatabaseNamesResult, 'database names');

  // Clean up
  let cleanUpPromises = [];
  for (let i = 0; i < 5; i++) {
    cleanUpPromises.push(dp.IndexedDB.deleteDatabase({storageKey, databaseName: "test-database" + i}));
  }
  await Promise.all(cleanUpPromises);

  testRunner.completeTest();
})
