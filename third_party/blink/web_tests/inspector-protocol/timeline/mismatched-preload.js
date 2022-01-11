(async function(testRunner) {
  const numberOfURLs = 2;

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests reporting of mismatched preloads in traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing("blink.resource");
  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add a preload
      const preload = document.createElement("link");
      preload.href = "../resources/empty.js";
      preload.rel = "preload";
      preload.as = "script";
      document.head.appendChild(preload);

      // Try to use it, but with the wrong CORS mode
      const script = document.createElement("script");
      script.src = "../resources/empty.js";
      script.crossOrigin = "anonymous";
      document.head.appendChild(script);
    })();
  `);

  // Wait for traces to show up.
  for (let i = 0; i < numberOfURLs; ++i) {
    await dp.Network.onceRequestWillBeSent();
  }

  const events = await tracingHelper.stopTracing(/blink.resource/);
  const requestEvents = events.filter(e => e.name == "ResourceFetcher::PrintPreloadMismatch");

  const resources = new Map();
  for (let e of requestEvents) {
    const url = e['args']['url'];
    const status = e['args']['MatchStatus'];
    const pathname = new URL(url).pathname;
    const path_array = pathname.split('/');
    resources.set(path_array[path_array.length - 1], status);
  }
  for (const resource of Array.from(resources.keys()).sort()) {
    testRunner.log(`${resource}: ${resources.get(resource)}`);
  }
  testRunner.completeTest();
})

