(async function(testRunner) {
  const numberOfURLs = 1;

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests reporting of unused preloads in traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing("blink.resource");
  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add an unused preload
      const preload = document.createElement("link");
      preload.href = "../resources/empty.js";
      preload.rel = "preload";
      preload.as = "script";
      document.head.appendChild(preload);
    })();
  `);

  // Wait for traces to show up.
  for (let i = 0; i < numberOfURLs; ++i) {
    await dp.Network.onceRequestWillBeSent();
  }

  // Wait for 5 seconds for the trace to appear.
  await new Promise(r => setTimeout(r, 3500));

  const events = await tracingHelper.stopTracing(/blink.resource/);
  const requestEvents = events.filter(e => e.name == "ResourceFetcher::WarnUnusedPreloads");

  const resources = new Map();
  for (let e of requestEvents) {
    const url = e['args']['url'];
    const pathname = new URL(url).pathname;
    const path_array = pathname.split('/');
    resources.set(path_array[path_array.length - 1], "");
  }
  for (const resource of Array.from(resources.keys()).sort()) {
    testRunner.log(`${resource}`);
  }
  testRunner.completeTest();
})
