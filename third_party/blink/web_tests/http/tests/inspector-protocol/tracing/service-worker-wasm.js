(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests V8 code cache for WebAssembly resources using Service Workers');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();

  // Register Service Worker and wait for activation.
  await session.evaluateAsync(`
    (async function() {
      const reg = await navigator.serviceWorker.register('/devtools/service-workers/resources/wasm-cache-worker.js', {
        scope: '/devtools/service-workers/resources/wasm-cache-iframe.html'
      });
      if (reg.active) return;
      await new Promise(resolve => {
        const worker = reg.installing || reg.waiting;
        if (!worker) return resolve();
        worker.addEventListener('statechange', () => {
          if (worker.state === 'activated') resolve();
        });
      });
    })()
  `);

  // Start tracing directly via CDP.
  await tracingHelper.startTracing('v8,v8.wasm,disabled-by-default-v8.wasm');

  // Load the iframe which fetches large.wasm twice to produce code cache hits.
  await session.evaluateAsync(`
    new Promise(resolve => {
      window.addEventListener('message', e => {
        if (e.data === 'done') resolve();
      }, {once: true});
      const iframe = document.createElement('iframe');
      iframe.src = '/devtools/service-workers/resources/wasm-cache-iframe.html';
      document.body.appendChild(iframe);
    })
  `);

  const events = await tracingHelper.stopTracing(/v8|wasm/);

  const targetNames = new Set([
    'v8.wasm.streamFromResponseCallback',
    'v8.wasm.compiledModule',
    'v8.wasm.cachedModule',
    'v8.wasm.moduleCacheHit',
    'v8.wasm.moduleCacheInvalid',
    'wasm.SyncCompile',
    'wasm.AsyncCompile',
    'wasm.GetNativeModuleFromCache',
    'CacheHit',
  ]);

  const matchedEvents = events.filter(e => targetNames.has(e.name));
  matchedEvents.sort((a, b) => {
    const urlA = a.args?.url || '';
    const urlB = b.args?.url || '';
    if (urlA !== urlB) return urlA.localeCompare(urlB);
    return a.name.localeCompare(b.name);
  });

  // V8 emits multiple CacheHit trace markers during streaming compilation
  // (GetStreamingCompilationOwnership and MaybeGetNativeModule). On heavily
  // loaded bots, GC or thread scheduling can cause the second marker to not
  // fire if the module in NativeModuleCache is evicted before the stream ends.
  // We only require at least one CacheHit event to verify the cache path.
  let seenCacheHit = false;
  const filteredEvents = matchedEvents.filter(event => {
    if (event.name === 'CacheHit') {
      if (seenCacheHit)
        return false;
      seenCacheHit = true;
    }
    return true;
  });

  for (const event of filteredEvents) {
    testRunner.log(`Event name: ${event.name}`);
    testRunner.log('Event shape:');
    tracingHelper.logEventShape(event);
    testRunner.log('');
  }

  testRunner.completeTest();
});
