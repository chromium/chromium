// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Network.setCacheDisabled affects the signed exchange certificate fetch request.\n`);

  const outerUrl =
      'https://127.0.0.1:8443/loading/sxg/resources/sxg-location.sxg';
  const certUrl =
      'https://127.0.0.1:8443/loading/sxg/resources/127.0.0.1.sxg.pem.cbor';
  const innerUrl =
      'https://127.0.0.1:8443/loading/sxg/resources/inner-url.html';

  // Mirror the DevTools front-end: apply the cache setting to every target
  // and track the requests of the page and its out-of-process iframes.
  let cacheDisabled = false;
  const sessions = [session];
  let requests = [];
  const requestsById = new Map();
  const loadingFinishedListeners = [];

  function listen(targetSession) {
    const protocol = targetSession.protocol;
    protocol.Network.onRequestWillBeSent(({params}) => {
      // The signed exchange inner response reuses the outer request id, so
      // treat every requestWillBeSent as a new request.
      const request = {url: params.request.url, cached: false};
      requestsById.set(params.requestId, request);
      requests.push(request);
    });
    protocol.Network.onRequestServedFromCache(({params}) => {
      const request = requestsById.get(params.requestId);
      if (request) {
        request.cached = true;
      }
    });
    protocol.Network.onResponseReceived(({params}) => {
      const request = requestsById.get(params.requestId);
      if (request && params.response.fromDiskCache) {
        request.cached = true;
      }
    });
    protocol.Network.onLoadingFinished(({params}) => {
      const request = requestsById.get(params.requestId);
      if (request) {
        for (const listener of loadingFinishedListeners) {
          listener(request);
        }
      }
    });
  }

  listen(session);
  dp.Target.onAttachedToTarget(async ({params}) => {
    const child = session.createChild(params.sessionId);
    sessions.push(child);
    listen(child);
    await Promise.all([
      child.protocol.Network.enable(),
      child.protocol.Network.setCacheDisabled({cacheDisabled}),
      child.protocol.Target.setAutoAttach(
          {autoAttach: true, waitForDebuggerOnStart: true, flatten: true}),
      child.protocol.Runtime.runIfWaitingForDebugger(),
    ]);
  });
  await dp.Network.enable();
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await setCacheDisabled(false);

  // Load the test signed exchange first, to cache the certificate file.
  await addIframe(outerUrl);

  requests = [];

  await setCacheDisabled(true);
  await addIframe(outerUrl + '?iframe-1');
  await addPrefetchAndWait(outerUrl + '?prefetch-1', innerUrl);

  await setCacheDisabled(false);
  // With the cache re-enabled `iframe-2` should have a cached request.
  await addIframe(outerUrl + '?iframe-2');
  // The prefetch has a different cache partition key and so will not be cached.
  await addPrefetchAndWait(outerUrl + '?prefetch-2', innerUrl);

  for (const request of requests) {
    if (request.url !== certUrl) {
      continue;
    }
    testRunner.log(`* ${request.url}`);
    testRunner.log(`  cached: ${request.cached}`);
  }
  testRunner.completeTest();

  async function setCacheDisabled(disabled) {
    cacheDisabled = disabled;
    await Promise.all(sessions.map(
        s => s.protocol.Network.setCacheDisabled({cacheDisabled})));
  }

  async function addIframe(url) {
    await session.evaluateAsync(url => new Promise(resolve => {
                                  const iframe =
                                      document.createElement('iframe');
                                  iframe.src = url;
                                  iframe.onload = resolve;
                                  document.body.appendChild(iframe);
                                }),
                                url);
  }

  async function addPrefetchAndWait(prefetchUrl, waitUrl) {
    const finished = new Promise(resolve => {
      const listener = request => {
        if (request.url !== waitUrl) {
          return;
        }
        loadingFinishedListeners.splice(
            loadingFinishedListeners.indexOf(listener), 1);
        resolve();
      };
      loadingFinishedListeners.push(listener);
    });
    await session.evaluate(url => {
      const link = document.createElement('link');
      link.rel = 'prefetch';
      link.href = url;
      document.body.appendChild(link);
    }, prefetchUrl);
    await finished;
  }
});
