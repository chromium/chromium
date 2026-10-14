// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared helper for the service-worker-network-fetch*.js tests. Registers
// resources/network-fetch-worker.js, asks it to fetch `url` and prints the
// network requests reported by the service worker target. If
// `blockedURLPatterns` is given, they are blocked in the service worker target
// with Network.setBlockedURLs before the worker starts.
(async function makeFetchInServiceWorker(testRunner, session,
                                         {scope, url, blockedURLPatterns}) {
  const dp = session.protocol;
  const requests = [];
  const requestsById = new Map();
  let resolveFetchFinished;
  const fetchFinished = new Promise(r => resolveFetchFinished = r);

  dp.Target.onAttachedToTarget(async ({params}) => {
    const swdp = session.createChild(params.sessionId).protocol;
    swdp.Network.onRequestWillBeSent(({params}) => {
      const request = {
        url: params.request.url,
        type: (params.type || 'Other').toLowerCase(),
        failed: false,
        blockedReason: undefined,
      };
      requests.push(request);
      requestsById.set(params.requestId, request);
    });
    swdp.Network.onLoadingFinished(({params}) => {
      if (requestsById.get(params.requestId)?.url === url) {
        resolveFetchFinished();
      }
    });
    swdp.Network.onLoadingFailed(({params}) => {
      const request = requestsById.get(params.requestId);
      if (!request) {
        return;
      }
      request.failed = true;
      request.blockedReason = params.blockedReason;
      if (request.url === url) {
        resolveFetchFinished();
      }
    });
    // Commands handled by the worker only respond once it runs, so issue them
    // in order and resume the worker before awaiting them.
    const commands = [swdp.Network.enable()];
    if (blockedURLPatterns) {
      commands.push(swdp.Network.setBlockedURLs({
        urlPatterns:
            blockedURLPatterns.map(urlPattern => ({urlPattern, block: true})),
      }));
    }
    commands.push(swdp.Runtime.runIfWaitingForDebugger());
    await Promise.all(commands);
  });
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const result = await session.evaluateAsync(async (scope, url) => {
    const registration = await navigator.serviceWorker.register(
        '/inspector-protocol/service-worker/resources/network-fetch-worker.js',
        {scope});
    const worker = registration.installing;
    return new Promise(resolve => {
      navigator.serviceWorker.onmessage = e => resolve(e.data);
      worker.postMessage({url, init: {}});
    });
  }, scope, url);
  await fetchFinished;

  testRunner.log('Fetch in worker result: ' + result);
  for (const request of requests) {
    testRunner.log(request.url);
    testRunner.log('resource.type: ' + request.type);
    testRunner.log('request.failed: ' + request.failed);
    if (blockedURLPatterns) {
      testRunner.log('request.blockedReason: ' + request.blockedReason);
    }
  }
})
