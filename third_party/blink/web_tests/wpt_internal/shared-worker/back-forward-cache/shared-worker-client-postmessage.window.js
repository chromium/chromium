// META: title=SharedWorker message while in bfcache should evict the entry.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js

'use strict';

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();

  const workerScriptUrl =
      '/wpt_internal/shared-worker/back-forward-cache/resources/worker-sending-message.js';

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc1.executeScript((workerUrl) => {
    return new Promise((resolve) => {
      const worker = new SharedWorker(workerUrl);
      worker.port.onmessage = (e) => {
        if (e.data === 'done')
          resolve();
      };
      worker.port.postMessage('register');
    });
  }, [workerScriptUrl]);

  await prepareForBFCache(rc1);
  const rc2 = await rc1.navigateToNew();
  await assertSimplestScriptRuns(rc2);

  const rcTrigger = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rcTrigger.executeScript((workerUrl) => {
    return new Promise((resolve) => {
      const triggerWorker = new SharedWorker(workerUrl);
      triggerWorker.port.onmessage = (e) => {
        if (e.data === 'done')
          resolve();
      };
      triggerWorker.port.postMessage('message');
    });
  }, [workerScriptUrl]);

  await rc2.historyBack();

  await assertNotRestoredFromBFCache(
      rc1, ['sharedworker-message'], ['sharedworker']);
});
