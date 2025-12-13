// META: title=BFCache is blocked and SharedWorkers are terminated correctly when all clients are navigated away.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js
// META: timeout=long

'use strict';

async function getWorkerId(remoteContext, workerName, workerUrl) {
  return remoteContext.executeScript((name, url) => {
    return new Promise(resolve => {
      const worker = new SharedWorker(url, {name: name});
      worker.port.onmessage = e => resolve(e.data.id);
      worker.port.postMessage('get_id');
    });
  }, [workerName, workerUrl]);
}

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();
  const workerUrl =
      '/wpt_internal/shared-worker/back-forward-cache/resources/stateful-worker.js';

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc1.executeScript(url => {
    new SharedWorker(url, {name: 'workerA'});
    new SharedWorker(url, {name: 'workerB'});
  }, [workerUrl]);

  const rc2 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc2.executeScript(url => {
    new SharedWorker(url, {name: 'workerA'});
    new SharedWorker(url, {name: 'workerC'});
  }, [workerUrl]);

  const rc3 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc3.executeScript(url => {
    new SharedWorker(url, {name: 'workerB'});
  }, [workerUrl]);

  const initial_id_A = await getWorkerId(rc1, 'workerA', workerUrl);
  const initial_id_B = await getWorkerId(rc1, 'workerB', workerUrl);
  const initial_id_C = await getWorkerId(rc2, 'workerC', workerUrl);

  // Navigate rc1 away, then navigate rc2 away to trigger the eviction logic.
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

  // Check which workers are still alive.
  const rc4 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  const final_id_A = await getWorkerId(rc4, 'workerA', workerUrl);
  const final_id_B = await getWorkerId(rc4, 'workerB', workerUrl);
  const final_id_C = await getWorkerId(rc4, 'workerC', workerUrl);

  assert_not_equals(
      final_id_A, initial_id_A,
      'Worker A should have been terminated and restarted.');
  assert_not_equals(
      final_id_C, initial_id_C,
      'Worker C should have been terminated and restarted.');
  assert_equals(final_id_B, initial_id_B, 'Worker B should still be alive.');
});
