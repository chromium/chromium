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

// Verifies that Client.postMessage() from a Service Worker to a BFCached client
// evicts the page, even when messages were previously queued via a transferred
// port. The test asserts the page is not restored from BFCache and that this
// process occurs without crashing, which was observed in crbug.com/431379643.
promise_test(async t => {
  // Register a service worker and make this page controlled.
  const {registration, page: pageA} =
      await createServiceWorkerControlledPage(t);

  await storeClients(t, registration.active);

  await pageA.executeScript(() => {
    return new Promise(resolve => {
      const channel = new MessageChannel();
      channel.port1.onmessage = e => {
        if (e.data === 'Port stored') {
          resolve();
        }
      };
      navigator.serviceWorker.controller.postMessage(
          {type: 'storePort'}, [channel.port2]);
    });
  });

  await prepareForBFCache(pageA);
  const pageANavigatedAway = await pageA.navigateToNew();
  await assertSimplestScriptRuns(pageANavigatedAway);

  // Trigger the SW to message the client via the previously transferred port.
  await postMessageViaTransferredPort(t, registration.active);

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
}, 'Direct Client.postMessage() to BFCached client should evict, no crash.');
