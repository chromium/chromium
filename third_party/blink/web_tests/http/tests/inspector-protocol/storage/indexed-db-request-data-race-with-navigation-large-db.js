(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Race IndexedDB requestData() with navigation using a large dataset.');

  const rounds = 2;
  const valueCount = 64;
  const valueSize = 8192;
  const securityOrigin = await session.evaluate('location.origin');

  await dp.Page.enable();
  await dp.Runtime.enable();
  await dp.IndexedDB.enable();

  // Repeat the test multiple times to trigger the race.
  for (let round = 0; round < rounds; ++round) {
    const dbName = `RaceDB_${round}`;

    // Navigate to an empty page to set up IndexedDB.
    await dp.Page.navigate(
        {url: securityOrigin + '/inspector-protocol/resources/empty.html'});
    await dp.Page.onceLoadEventFired();

    // Create a larger IndexedDB with KBs data to make requestData() take longer.
    await session.evaluateAsync(`(async () => {
      const dbName = ${JSON.stringify(dbName)};
      const storeName = 'TestObjectStore';
      const payload = 'X'.repeat(${valueSize});
      await new Promise((resolve, reject) => {
        const request = indexedDB.deleteDatabase(dbName);
        request.onsuccess = () => resolve();
        request.onerror = () => resolve();
        request.onblocked = () => reject(String(request.error || 'delete blocked'));
      });
      await new Promise((resolve, reject) => {
        const request = indexedDB.open(dbName, 1);
        request.onupgradeneeded = event => {
          const db = event.target.result;
          const store = db.createObjectStore(storeName, {keyPath: 'id'});
          for (let i = 0; i < ${valueCount}; ++i) {
            store.put({id: i, label: 'row-' + i, value: payload + i});
          }
        };
        request.onsuccess = () => {
          request.result.close();
          resolve();
        };
        request.onerror = () => reject(String(request.error || 'open failed'));
      });
      return 'done';
    })()`);

    // Start an async IndexedDB read.
    dp.IndexedDB.requestData({
      securityOrigin: securityOrigin,
      databaseName: dbName,
      objectStoreName: 'TestObjectStore',
      skipCount: 0,
      pageSize: valueCount,
    });

    // Navigate immediately to race against the pending requestData() callback.
    dp.Page.navigate(
        {url: 'data:text/html,<body>navigated<iframe></iframe></body>'});

    // Allow time for teardown to execute while the requestData() callback may still fire.
    await new Promise(resolve => setTimeout(resolve, 500));

    testRunner.log(`Round ${round} completed without a crash.`);
  }

  // Clean up the databases.
  await dp.Page.navigate(
      {url: securityOrigin + '/inspector-protocol/resources/empty.html'});
  await dp.Page.onceLoadEventFired();
  await session.evaluateAsync(`(async () => {
    for (let round = 0; round < ${rounds}; ++round) {
      await new Promise(resolve => {
        const request = indexedDB.deleteDatabase('RaceDB_' + round);
        request.onsuccess = () => resolve();
        request.onerror = () => resolve();
        request.onblocked = () => resolve();
      });
    }
    return 'done';
  })()`);

  testRunner.completeTest();
})
