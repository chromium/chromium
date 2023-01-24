(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that tracking and untracking IndexedDB for storage key works\n`);

  await dp.Page.enable();
  const protocolMessages = [];
  const originalDispatchMessage = DevToolsAPI.dispatchMessage;
  const originalSendCommand = DevToolsAPI._sendCommand;
  DevToolsAPI.dispatchMessage = (message) => {
    protocolMessages.push(message);
    originalDispatchMessage(message);
  };
  DevToolsAPI._sendCommand = (sessionId, method, params) => {
    protocolMessages.push({sessionId, method, params});
    return originalSendCommand(sessionId, method, params);
  }
  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) => testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => {
    testRunner.log(protocolMessages);
    testRunner.die('Timeout', errorForLog);
  }, 5000);

  const frameId = (await dp.Page.getResourceTree()).result.frameTree.frame.id;
  errorForLog = new Error();
  const storageKey = (await dp.Storage.getStorageKeyForFrame({
                       frameId: frameId
                     })).result.storageKey;
  errorForLog = new Error();
  await dp.Storage.trackIndexedDBForStorageKey({storageKey});
  errorForLog = new Error();
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
  errorForLog = new Error();

  testRunner.log('\nUntrack IndexedDB for storage key');

  await dp.Storage.untrackIndexedDBForStorageKey({storageKey});
  errorForLog = new Error();
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
  errorForLog = new Error();

  testRunner.log(oneMoreValue);

  // Clean up
  await dp.IndexedDB.deleteDatabase({storageKey, databaseName: "test-database"});
  errorForLog = new Error();

  testRunner.completeTest();
})
