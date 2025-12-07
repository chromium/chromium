// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/rc-helper.js
// META: script=/html/browsers/browsing-the-web/back-forward-cache/resources/helper.sub.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper-tests/resources/test-helper.js
// META: script=/service-workers/service-worker/resources/test-helpers.sub.js
// META: script=./resources/helper.js
// META: timeout=long

'use strict';

// Verifies that calling Client.postMessage() to a BFCached client evicts the
// page from the cache. The test asserts the page is not restored from BFCache
// on back navigation.
promise_test(async t => {
  // Register a service worker and make this page controlled.
  const {registration, page: pageA} =
      await createServiceWorkerControlledPage(t);

  // The rest of the test-specific logic follows.
  await storeClients(t, registration.active);

  await prepareForBFCache(pageA);
  const pageANavigatedAway = await pageA.navigateToNew();
  await assertSimplestScriptRuns(pageANavigatedAway);

  // Posting a message to a client should evict it from the bfcache.
  await postMessageToStoredClients(t, registration.active);

  // Back navigate and check whether the page is restored from BFCache.
  await pageANavigatedAway.historyBack();
  await assertNotRestoredFromBFCache(
      pageA,
      ['serviceworker-postmessage'],
  );

  // After the page reloads, ensure it's still controlled by the SW.
  await pageA.executeScript(() => navigator.serviceWorker.ready);
  assert_true(
      await pageA.executeScript(
          () => (navigator.serviceWorker.controller !== null)),
      'pageA should be controlled after history navigation');
}, 'Client.postMessage while a controlled page is in BFCache');
