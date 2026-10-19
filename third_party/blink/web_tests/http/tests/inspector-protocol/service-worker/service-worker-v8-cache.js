// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      `Tests that v8.compile and v8.produceCache trace events report code cache usage for scripts stored in Service Worker Cache Storage.\n`);

  const resources = '/inspector-protocol/service-worker/resources/';
  const scriptURL = resources + 'v8-cache-worker.js';
  const scope = resources + 'v8-cache-iframe.html';
  const cachedScriptURL = '/devtools/resources/v8-cache-script.js';

  // Trace on the browser target so that events from the service worker are
  // captured regardless of the renderer process it is started in.
  const bp = testRunner.browserP();

  async function traceAndDumpCodeCacheEvents(action) {
    const events = [];
    const onDataCollected = ({params}) => events.push(...params.value);
    bp.Tracing.onDataCollected(onDataCollected);
    await bp.Tracing.start(
        {categories: '-*,devtools.timeline', type: '', options: ''});
    await action();
    bp.Tracing.end();
    await bp.Tracing.onceTracingComplete();
    bp.Tracing.offDataCollected(onDataCollected);

    const codeCacheEvents =
        events
            .filter(e => (e.name === 'v8.compile' ||
                          e.name === 'v8.produceCache') &&
                        e.args?.data?.url?.includes('v8-cache'))
            .sort((a, b) => a.ts - b.ts);
    for (const event of codeCacheEvents) {
      const data = {};
      for (const key of Object.keys(event.args.data).sort()) {
        const value = event.args.data[key];
        switch (key) {
          case 'scriptId':
            continue;
          case 'url':
            data[key] = value.replace(/^https?:\/\/[^/]+/, '...');
            break;
          // Whether streaming is used depends on timing.
          case 'streamed':
          case 'notStreamedReason':
          case 'consumedCacheSize':
          case 'producedCacheSize':
            data[key] = `<${typeof value}>`;
            break;
          default:
            data[key] = value;
        }
      }
      testRunner.log(data, `${event.name}: `);
    }
  }

  testRunner.log('--- Trace events while installing -------------');
  await traceAndDumpCodeCacheEvents(async () => {
    await session.evaluateAsync(`
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
  });
  testRunner.log('-----------------------------------------------');

  await session.evaluateAsync(scope => new Promise(resolve => {
                                const iframe = document.createElement('iframe');
                                iframe.id = 'frame_id';
                                iframe.src = scope;
                                iframe.onload = resolve;
                                document.body.appendChild(iframe);
                              }),
                              scope);

  testRunner.log('--- Trace events while executing scripts ------');
  await traceAndDumpCodeCacheEvents(async () => {
    await session.evaluateAsync(async url => {
      const iframeWindow = document.getElementById('frame_id').contentWindow;
      await iframeWindow.loadScript(url);
      await iframeWindow.loadScript(url);
    }, cachedScriptURL);
  });
  testRunner.log('-----------------------------------------------');

  testRunner.completeTest();
});
