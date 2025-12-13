(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that IndexedDB protocol commands work in a service worker.');

  const dbName = 'TestDB_Service';
  const securityOrigin = await session.evaluate('location.origin');

  const bp = testRunner.browserP();

  await bp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const attachedPromise = bp.Target.onceAttachedToTarget(
      e => e.params.targetInfo.type === 'service_worker');

  const readyPromise = session.evaluateAsync(`
    new Promise(resolve => {
      navigator.serviceWorker.onmessage = resolve;
      navigator.serviceWorker.register(
        '${testRunner.url('../resources/indexed-db-service-worker.js')}'
      );
    });
  `);

  const [attachedEvent] = await Promise.all([attachedPromise, readyPromise]);

  const workerDP = session.createChild(attachedEvent.params.sessionId).protocol;

  await workerDP.IndexedDB.enable();
  await workerDP.Runtime.enable();

  const dbNamesResult =
      await workerDP.IndexedDB.requestDatabaseNames({securityOrigin});
  const dbNameFound = dbNamesResult.result.databaseNames.includes(dbName);
  testRunner.log(`Database ${dbName} found: ${dbNameFound}`);

  const dataResult = await workerDP.IndexedDB.requestData({
    securityOrigin: securityOrigin,
    databaseName: dbName,
    objectStoreName: 'TestObjectStore',
    skipCount: 0,
    pageSize: 10,
  });

  const remoteObject = dataResult.result.objectStoreDataEntries[0].value;
  const propsResult =
      await workerDP.Runtime.getProperties({objectId: remoteObject.objectId});

  testRunner.log('Value object properties:');
  for (const prop of propsResult.result.result) {
    if (prop.enumerable) {
      testRunner.log(`  - ${prop.name}: ${prop.value.value}`);
    }
  }

  await session.evaluateAsync(dbName => {
    return new Promise(resolve => {
      const request = indexedDB.deleteDatabase(dbName);
      request.onsuccess = () => resolve();
      request.onerror = () => resolve();
      request.onblocked = () => resolve();
    });
  }, dbName);

  testRunner.completeTest();
})
