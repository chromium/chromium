(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that tracking and untracking IndexedDB for storage key works\n`);

  await dp.Page.enable();

  testRunner.startDumpingProtocolMessages();

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  const storageKey = (await dp.Storage.getStorageKeyForFrame({
                       frameId: frameId
                     })).result.storageKey;
  await dp.Storage.trackIndexedDBForStorageKey({storageKey});
  const listUpdatedPromise = dp.Storage.onceIndexedDBListUpdated(
      message => {return `indexedDB list updated for storage key ${
          message.params.storageKey}`});
  const contentUpdatedPromise = dp.Storage.onceIndexedDBContentUpdated(
      message => {return `indexedDB content updated for ${message.params}`});

  testRunner.log(`Open database, object store and set value`);

  // Create database, objectStore and add a key-value pair.
  const valuePromise = session.evaluateAsync(`
    new Promise(async resolve => {
      const request = window.indexedDB.open("test-database");
      request.onerror = (event) => {
        resolve('failed to create a database');
      };
      request.onupgradeneeded = (event) => {
        const db = event.target.result;
        const objectStore = db.createObjectStore("test-store");
        objectStore.add("test-data", "test-key");
        resolve('key-value pair added successfully');
      };
    })
  `);

  testRunner.log(await Promise.all(
      [listUpdatedPromise, contentUpdatedPromise, valuePromise]));

  testRunner.log('\nUntrack IndexedDB for storage key');

  await dp.Storage.untrackIndexedDBForStorageKey({storageKey});
  dp.Storage.onIndexedDBListUpdated(message => {message.params.storageKey});
  dp.Storage.onIndexedDBContentUpdated(message => {message.params});

  testRunner.log('\nAdd one more value')

  // Open database, objectStore and add another value.
  const oneMoreValue = await session.evaluateAsync(`
    new Promise(async resolve => {
      const openreq = window.indexedDB.open("test-database");
      openreq.onerror = (event) => {
        resolve("not able to open database");
      }
      openreq.onsuccess = (event) => {
        const db = event.target.result;
        const store = db.transaction(['test-store'],'readwrite').objectStore('test-store');
        store.add("one-more-test-data", "one-more-test-key");
        resolve("one more key-value pair added");
      };
    })
  `);

  testRunner.log(oneMoreValue);

  // Clean up
  await dp.IndexedDB.deleteDatabase({storageKey, databaseName: "test-database"});

  testRunner.completeTest();
})
