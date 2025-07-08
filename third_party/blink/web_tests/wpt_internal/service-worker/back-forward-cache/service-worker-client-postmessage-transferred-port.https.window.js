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

// Verifies that a message sent via a transferred port to a BFCached page is
// queued and does not cause eviction. The test asserts the page is restored
// from BFCache, and that the 'pageshow' event fires before the queued message
// is processed.
promise_test(async t => {
  // Register a service worker and make this page controlled.
  const {registration, page: pageA} =
      await createServiceWorkerControlledPage(t);

  await pageA.executeScript(() => {
    return new Promise(resolve => {
      window.eventLog = [];
      window.addEventListener('pageshow', event => {
        if (event.persisted) {
          window.eventLog.push('pageshow');
        }
      });

      const channel = new MessageChannel();
      channel.port1.onmessage = e => {
        if (e.data === 'Port stored') {
          resolve();
        } else {
          window.eventLog.push('sw-message-received');
        }
      };

      navigator.serviceWorker.controller.postMessage(
          {
            type: 'storePort',
          },
          [channel.port2]);
    });
  });

  await prepareForBFCache(pageA);
  const pageANavigatedAway = await pageA.navigateToNew();
  await assertSimplestScriptRuns(pageANavigatedAway);

  // Trigger the SW to message the client via the previously transferred port.
  await postMessageViaTransferredPort(t, registration.active);

  // Assert that the page was successfully restored from BFCache.
  await pageANavigatedAway.historyBack();
  await assertImplementsBFCacheOptional(pageA);

  // Retrieve the event log from `pageA` and verify the order of events.
  const eventLog = await pageA.executeScript(() => window.eventLog);
  assert_array_equals(
      eventLog, ['pageshow', 'sw-message-received'],
      'The pageshow event must fire before the queued SW message is processed.');
}, 'Message from SW to a BFCached page via transferred port should be queued and processed after restore.');
