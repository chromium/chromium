(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
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
  };
  window.onerror = (msg) => testRunner.log('onerror: ' + msg);
  window.onunhandledrejection = (e) => testRunner.log('onunhandledrejection: ' + e.reason);
  let errorForLog = new Error();
  setTimeout(() => {
    testRunner.log(protocolMessages);
    testRunner.die('Took longer than 25s', errorForLog);
  }, 25000);

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
  const id = Math.random();

  // Create database, objectStore and add a key-value pair.
  const valuePromise = session.evaluateAsync(`
    new Promise(async resolve => {
      const request = window.indexedDB.open("test-database${id}");
      request.onerror = (event) => {
        resolve('failed to create a database');
      };
      request.onupgradeneeded = (event) => {
        const db = event.target.result;
        const objectStore = db.createObjectStore("test-store");
        objectStore.add("test-data", "test-key");
        resolve('key-value pair added successfully');
        db.close();
      };
    })
  `);

  const [listUpdatedEvent, contentUpdatedEvent, value] = await Promise.all(
    [listUpdatedPromise, contentUpdatedPromise, valuePromise]);
  testRunner.log(
      listUpdatedEvent, '', ['databaseName', 'sessionId', 'bucketId']);
  testRunner.log(
      contentUpdatedEvent, '', ['databaseName', 'sessionId', 'bucketId']);
  testRunner.log(value);
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
      const openreq = window.indexedDB.open("test-database${id}");
      openreq.onerror = (event) => {
        resolve("not able to open database");
      }
      openreq.onsuccess = (event) => {
        const db = event.target.result;
        const store = db.transaction(['test-store'],'readwrite').objectStore('test-store');
        store.add("one-more-test-data", "one-more-test-key");
        resolve("one more key-value pair added");
        db.close();
      };
    })
  `);
  errorForLog = new Error();

  testRunner.log(oneMoreValue);

    // Clean up
  try {
    await session.evaluateAsync(`
      new Promise(async (resolve, reject) => {
        const req = window.indexedDB.deleteDatabase("test-database${id}");
        req.onsuccess = resolve;
        req.onerror = reject;
      });
    `);
  } catch (e) {
    testRunner.log(e);
  } finally {
    testRunner.completeTest();
  }

})
