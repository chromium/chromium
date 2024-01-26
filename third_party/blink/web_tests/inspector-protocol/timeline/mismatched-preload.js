(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const numberOfURLs = 2;

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests reporting of mismatched preloads in traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing("blink.resource,devtools.timeline");
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

  const events = await tracingHelper.stopTracing(/blink.resource|devtools.timeline/);
  const networkEvents = events.filter(e => e.name == "ResourceSendRequest");
  const requestEvents = events.filter(e => e.name == "ResourceFetcher::PrintPreloadMismatch");

  function truncate(url) {
    const pathname = new URL(url).pathname;
    const path_array = pathname.split('/');
    return path_array[path_array.length - 1];
  };
  const resources = new Map();
  for (let e of requestEvents) {
    const data = e['args']['data'];
    const url = truncate(data['url']);
    const status = data['status'];
    resources.set(url, {'status': status, 'id': data['requestId']});
  }
  const seen = new Set();
  for (let e of networkEvents) {
    const data = e['args']['data'];
    const url = truncate(data['url']);
    if (seen.has(url)) {
      // We only compare the IDs of the first request, as this is the mismatched preload.
      continue;
    }
    seen.add(url);
    const id = data['requestId'];
    if (resources.get(url)['id'] !== id) {
      testRunner.log("ID mismatch " + resources.get(url)['id'] + "!=" + id);
    } else {
      testRunner.log("Matching ID");
    }
  }
  for (const resource of Array.from(resources.keys()).sort()) {
    testRunner.log(`${resource}: ${resources.get(resource)["status"]}`);
  }
  testRunner.completeTest();
})

