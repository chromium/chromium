// META: title=BFCache is blocked when a frame connected to multiple different SharedWorkers is the last client.
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

  // Create two client connections to the different SharedWorker instance.
  await rc1.executeScript(url => {
    window.workerA = new SharedWorker(url, 'workerA');
    window.workerB = new SharedWorker(url, 'workerB');
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
