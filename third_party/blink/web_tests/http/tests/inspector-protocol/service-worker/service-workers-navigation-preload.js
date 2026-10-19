// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that Network events for navigation preload requests are reported on the service worker target.\n`);

  const resources =
      'http://127.0.0.1:8000/inspector-protocol/service-worker/resources/';
  const scriptURL = resources + 'navigation-preload-worker.js';
  const scope = resources + 'navigation-preload-scope.php';

  // Navigation preload requests are only reported to the service worker
  // target when the navigation itself is instrumented by DevTools.
  await dp.Network.enable();
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  const attachedPromise = dp.Target.onceAttachedToTarget(
      event => event.params.targetInfo.type === 'service_worker');
  const activatedPromise = session.evaluateAsync(`
    (async function() {
      const reg = await navigator.serviceWorker.register(
          '${scriptURL}', {scope: '${scope}'});
      const worker = reg.installing || reg.waiting || reg.active;
      if (worker.state === 'activated')
        return;
      await new Promise(resolve => {
        worker.addEventListener('statechange', () => {
          if (worker.state === 'activated')
            resolve();
        });
      });
    })()`);
  const attached = await attachedPromise;
  const swSession = session.createChild(attached.params.sessionId);
  const swdp = swSession.protocol;
  await Promise.all([
    swdp.Network.enable(),
    swdp.Runtime.runIfWaitingForDebugger(),
  ]);
  await activatedPromise;

  const preloadRequestIds = new Set();
  let resolveFinished;
  swdp.Network.onRequestWillBeSent(event => {
    const {requestId, request, initiator} = event.params;
    if (initiator.type !== 'preload') {
      return;
    }
    preloadRequestIds.add(requestId);
    testRunner.log('requestWillBeSent:');
    testRunner.log('  url: ' + request.url);
    const preloadHeader = request.headers['Service-Worker-Navigation-Preload'];
    testRunner.log(`  request.headers['Service-Worker-Navigation-Preload']: ${
        preloadHeader}`);
  });
  swdp.Network.onResponseReceived(event => {
    const {requestId, response} = event.params;
    if (!preloadRequestIds.has(requestId)) {
      return;
    }
    testRunner.log('responseReceived:');
    testRunner.log('  status: ' + response.status);
    testRunner.log('  timing available: ' + !!response.timing);
  });
  swdp.Network.onLoadingFinished(event => {
    if (!preloadRequestIds.has(event.params.requestId)) {
      return;
    }
    testRunner.log('loadingFinished');
    resolveFinished();
  });
  swdp.Network.onLoadingFailed(event => {
    if (!preloadRequestIds.has(event.params.requestId)) {
      return;
    }
    testRunner.log('loadingFailed:');
    testRunner.log('  errorText: ' + event.params.errorText);
    resolveFinished();
  });

  testRunner.log('-----------------');
  testRunner.log('Loading an iframe.');
  await addIframeAndWaitForPreload(scope);
  testRunner.log('-----------------');
  testRunner.log('Loading another iframe.');
  await addIframeAndWaitForPreload(scope + '?BrokenChunked');
  testRunner.log('-----------------');
  testRunner.log('Loading another iframe.');
  await addIframeAndWaitForPreload(scope + '?Redirect');
  testRunner.log('-----------------');
  testRunner.log('Done');
  testRunner.completeTest();

  async function addIframeAndWaitForPreload(url) {
    const finished = new Promise(resolve => resolveFinished = resolve);
    await session.evaluateAsync(url => new Promise(resolve => {
                                  const iframe =
                                      document.createElement('iframe');
                                  iframe.src = url;
                                  iframe.onload = resolve;
                                  document.body.appendChild(iframe);
                                }),
                                url);
    await finished;
    testRunner.log('The iframe loaded.');
  }
});
