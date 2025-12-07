// META: title=A Fenced Frame client correctly blocks its page from BFCache.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js
// META: script=/fenced-frame/resources/utils.js

'use strict';

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();
  const workerUrl =
      '/wpt_internal/shared-worker/back-forward-cache/resources/worker-empty.js';

  const rc1 = await rcHelper.addWindow(
      {scripts: ['/fenced-frame/resources/utils.js', '/common/utils.js']},
      {features: 'noopener'});

  await rc1.executeScript(async (workerUrl) => {
    // Create two Fenced Frames and connect it to the same SharedWorker.
    const fencedframe1 = await attachFencedFrameContext();
    const fencedframe2 = await attachFencedFrameContext();

    await fencedframe1.execute(async (workerUrl) => {
      window.worker = new SharedWorker(workerUrl);
    }, [workerUrl]);
    await fencedframe2.execute(async (workerUrl) => {
      window.worker = new SharedWorker(workerUrl);
    }, [workerUrl]);
  }, [workerUrl]);

  // Navigate the main frame away. Since the Fenced frames are the only frames
  // connected to the worker, it should be blocked from entering BFCache.
  await prepareForBFCache(rc1);
  const rc1Away = await rc1.navigateToNew();
  await assertSimplestScriptRuns(rc1Away);

  await rc1Away.historyBack();
  // The reason is 'masked' because Fenced Frames hide internal blocking
  // reasons.
  await assertNotRestoredFromBFCache(rc1, ['masked']);
});
