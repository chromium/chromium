(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startBlank(
    'Tests that IndexedDB protocol commands work in a frame.');

  const securityOrigin = await session.evaluate('location.origin');

  await session.evaluateAsync(`
    new Promise(resolve => {
      const request = indexedDB.open('TestDB', 1);
      request.onupgradeneeded = e => {
        const db = e.target.result;
        const store = db.createObjectStore('TestObjectStore', {keyPath: 'id'});
        store.put({id: 1, value: 'Value'});
      };
      request.onsuccess = () => {
        resolve('done');
      };
    });
  `);

  await dp.IndexedDB.enable();
  await dp.Runtime.enable();

  const dbNamesResult =
    await dp.IndexedDB.requestDatabaseNames({ securityOrigin });
  const dbName = dbNamesResult.result.databaseNames[0];
  dp.IndexedDB.requestData({
    securityOrigin: securityOrigin,
    databaseName: dbName,
    objectStoreName: 'TestObjectStore',
    skipCount: 0,
    pageSize: 10,
  });

  testRunner.log('Navigating page immediately to trigger potential UAF...');
  dp.Page.navigate({ url: 'data:text/html,<body>page<iframe></iframe></body>' });

  // This wait is crucial as it creates the specific race condition this test
  // is designed to detect. It provides a time window for the full teardown
  // sequence of the previous page context to execute. This sequence includes:
  //   - Destructing the underlying `content::WebContentsImpl`.
  //   - Destructing the underlying `content::DevToolsFrontendHostImpl`.
  //   - Detaching the DevToolsSession (`blink::DevToolsSession::Detach`).
  //   - Destructing the underlying `v8_inspector::V8InspectorSession`.
  //   - Garbage collecting and destructing the pending callback objects
  //     (like DataLoader and OpenCursorCallback).
  // The test checks if the asynchronous IndexedDB callback from Step 4 fires
  // *after* this teardown has completed. If a vulnerability exists, the
  // callback will attempt to access freed memory, resulting in a
  // Use-After-Free crash. If the code is correct, it will handle this
  // situation gracefully without crashing.
  await new Promise((resolve) => {
    setTimeout(resolve, 500);
  });

  testRunner.log('Wait completed without a crash.');
  testRunner.completeTest();
})
