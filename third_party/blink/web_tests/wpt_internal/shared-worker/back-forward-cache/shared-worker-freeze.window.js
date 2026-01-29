// META: title=SharedWorker can be frozen and restored from BFCache.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js
// META: script=resources/shared-worker-bfcache-helper.js

// Verifies that a SharedWorker is frozen when its client enters BFCache by
// detecting a large gap in execution timestamps. This "gap check" approach is
// chosen to avoid flakiness. Note that while a large gap confirms execution
// stopped, it technically permits false positives (e.g., due to system lag).
promise_test(async t => {
  const rcHelper = new RemoteContextHelper();
  const workerUrl =
      '/wpt_internal/shared-worker/back-forward-cache/resources/timestamp-worker.js';

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});
  await rc1.executeScript((workerUrl) => {
    window.worker = new SharedWorker(workerUrl);
  }, [workerUrl]);

  // Navigate away to a different page to fire the pagehide event.
  await prepareForBFCache(rc1);
  const rc1Away = await rc1.navigateToNew();
  await assertSimplestScriptRuns(rc1Away);

  // Wait to create a measurable execution gap to verify that the worker is
  // frozen.
  await new Promise(resolve => setTimeout(resolve, 1000));

  await rc1Away.historyBack();
  await assertImplementsBFCacheOptional(rc1);

  // Get the max gap between timestamps in the worker.
  const maxGap = await rc1.executeScript(async () => {
    const timestamps = await new Promise(resolve => {
      window.worker.port.onmessage = (e) => resolve(e.data);
      window.worker.port.postMessage('get_timestamps');
    });

    return timestamps.reduce((max, curr, i, arr) => {
      if (i === 0)
        return 0;
      return Math.max(max, curr - arr[i - 1]);
    }, 0);
  });

  // Confirm freezing by checking for a large gap.
  assert_greater_than(
      maxGap, 1000,
      'Worker should have frozen (stopped execution) for at least 1s while in BFCache.');
});
