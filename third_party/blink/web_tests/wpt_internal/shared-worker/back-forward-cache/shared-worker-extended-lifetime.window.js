// META: title=Extended lifetime SharedWorker can be alive if all its clients are in BFCache.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();
  const workerUrl =
      '/wpt_internal/shared-worker/back-forward-cache/resources/writer-worker.js';
  const DB_NAME = 'wpt-bfcache-test-db';
  const STORE_NAME = 'store';

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});

  await rc1.executeScript((workerUrl, dbName, storeName) => {
    window.worker = new SharedWorker(workerUrl, {extendedLifetime: true});
    window.addEventListener('pagehide', () => {
      // Send a message to the worker to write to the DB.
      window.worker.port.postMessage(
          {command: 'try_to_write', dbName: dbName, storeName: storeName});
    });
  }, [workerUrl, DB_NAME, STORE_NAME]);

  // Navigate away to a different page to fire the pagehide event.
  await prepareForBFCache(rc1);
  const rc1Away = await rc1.navigateToNew();
  await assertSimplestScriptRuns(rc1Away);

  // Wait and get the value written to the DB.
  async function checkValueInDB() {
    const result = await new Promise((resolve, reject) => {
      const request = indexedDB.open(DB_NAME, 1);
      request.onsuccess = e => {
        const db = e.target.result;
        const tx = db.transaction(STORE_NAME, 'readonly');
        tx.objectStore(STORE_NAME).get('key').onsuccess = e =>
            resolve(e.target.result);
      };
      request.onerror = e => reject(e.target.error);
    });
    return result === 'value';
  }
  await t.step_wait(
      checkValueInDB, 'Worker should have successfully written to DB.',
      30000, /* timeout */
      100    /* interval */
  );

  // Navigate back to restore the page with the worker from BFCache.
  await rc1Away.historyBack();
  await assertImplementsBFCacheOptional(rc1);
});
