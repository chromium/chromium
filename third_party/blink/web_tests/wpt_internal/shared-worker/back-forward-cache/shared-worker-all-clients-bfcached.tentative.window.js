// META: title=BFCache is blocked and existing entries are evicted when all clients are BFCached.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js

'use strict';

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc1.executeScript(async () => {
    window.worker = new SharedWorker(
        '/wpt_internal/shared-worker/back-forward-cache/resources/worker-empty.js');
  });

  const rc2 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc2.executeScript(async () => {
    window.worker = new SharedWorker(
        '/wpt_internal/shared-worker/back-forward-cache/resources/worker-empty.js');
  });

  // Check that rc1 can be BFCached since there is still an active client.
  await assertBFCacheEligibility(rc1, /*shouldRestoreFromBfcache=*/ true);

  await prepareForBFCache(rc1);
  const rc1Away = await rc1.navigateToNew();
  await assertSimplestScriptRuns(rc1Away);

  await prepareForBFCache(rc2);
  const rc2Away = await rc2.navigateToNew();
  await assertSimplestScriptRuns(rc2Away);

  // Neither should be restored from BFCache, since a SharedWorker cannot be
  // left with no active clients. rc1 is evicted, and rc2 is blocked.
  await rc1Away.historyBack();
  await assertNotRestoredFromBFCache(
      rc1, ['sharedworker-with-no-active-client'], ['sharedworker']);
  await rc2Away.historyBack();
  await assertNotRestoredFromBFCache(
      rc2, ['sharedworker-with-no-active-client'], ['sharedworker']);
});
