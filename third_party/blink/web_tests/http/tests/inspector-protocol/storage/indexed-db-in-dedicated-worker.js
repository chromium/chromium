(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that IndexedDB protocol commands work in a dedicated worker.');

  const dbName = 'TestDB_Dedicated';
  const securityOrigin = await session.evaluate('location.origin');

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget(
      e => e.params.targetInfo.type === 'worker');

  const readyPromise = session.evaluateAsync(`
    new Promise(resolve => {
      const worker = new Worker('${
      testRunner.url('../resources/indexed-db-dedicated-worker.js')}');
      worker.onmessage = resolve;
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
