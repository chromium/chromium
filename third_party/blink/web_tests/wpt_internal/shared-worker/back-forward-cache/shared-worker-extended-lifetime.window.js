// META: title=Extended lifetime SharedWorker can be alive if all its clients are in BFCache.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js
// META: script=resources/shared-worker-bfcache-helper.js

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();
  const workerUrl =
      '/wpt_internal/shared-worker/back-forward-cache/resources/writer-worker.js';
  const DB_NAME = 'wpt-bfcache-extended-test' + Math.random();
  const STORE_NAME = 'store' + Math.random();
  const KEY = 'test_key' + Math.random();
  const VALUE = 'test_value' + Math.random();

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  const bc = new BroadcastChannel('shared-worker-bfcache-test');

  await rc1.executeScript((workerUrl) => {
    window.worker = new SharedWorker(workerUrl, {extendedLifetime: true});
  }, [workerUrl]);

  // Navigate away to a different page to fire the pagehide event.
  await prepareForBFCache(rc1);
  const rc1Away = await rc1.navigateToNew();
  await assertSimplestScriptRuns(rc1Away);

  // Try to write to the DB from the worker.
  bc.postMessage({
    command: 'try_to_write',
    dbName: DB_NAME,
    storeName: STORE_NAME,
    key: KEY,
    value: VALUE
  });
  await new Promise((resolve) => {
    bc.onmessage = e => {
      if (e.data === 'wrote_to_db')
        resolve();
    };
  });

  // Get the value written to the DB.
  const result = await getValueFromIndexedDB(DB_NAME, STORE_NAME, KEY);
  assert_equals(result, VALUE);

  // Navigate back to restore the page with the worker from BFCache.
  await rc1Away.historyBack();
  await assertImplementsBFCacheOptional(rc1);
});
