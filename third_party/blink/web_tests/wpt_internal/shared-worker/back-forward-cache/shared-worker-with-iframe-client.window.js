// META: title=BFCache is blocked for a page with an iframe that is also a SharedWorker client.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js

'use strict';

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();
  const workerUrl =
      '/wpt_internal/shared-worker/back-forward-cache/resources/worker-empty.js';

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc1.executeScript((workerUrl) => {
    window.worker = new SharedWorker(workerUrl);
  }, [workerUrl]);

  const iframe = await rc1.addIframe();
  await iframe.executeScript((workerUrl) => {
    new SharedWorker(workerUrl);
  }, [workerUrl]);

  // Navigate rc1 away. Since this is the only document connected to the worker,
  // it should be blocked from entering BFCache.
  await prepareForBFCache(rc1);
  const rc1Away = await rc1.navigateToNew();
  await assertSimplestScriptRuns(rc1Away);

  await rc1Away.historyBack();
  await assertNotRestoredFromBFCache(
      rc1, ['sharedworker-with-no-active-client']);
});
