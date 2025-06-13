// META: title=SharedWorker blocks bfcache.
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js
// META: timeout=long

'use strict';

promise_test(async t => {
  const rcHelper = new RemoteContextHelper();

  const rc1 = await rcHelper.addWindow(
      /*config=*/ {}, /*options=*/ {features: 'noopener'});

  await rc1.executeScript(async () => {
    window.foo = new SharedWorker(
        '/wpt_internal/shared-worker/back-forward-cache/resources/worker-empty.js');
  });
  await assertBFCacheEligibility(rc1, /*shouldRestoreFromBfcache=*/ false);
  await assertNotRestoredFromBFCache(
      rc1,
      ['sharedworker'],
  );
});
