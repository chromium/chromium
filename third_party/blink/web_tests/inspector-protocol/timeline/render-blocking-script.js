(async function(testRunner) {
  const numberOfURLs = 3;

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests render blocking status in script traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add a dynamic script
      const script = document.createElement("script");
      script.src = "../resources/empty.js?dynamic";
      document.head.appendChild(script);

      // Add a dynamic non-async script
      const non_async_script = document.createElement("script");
      non_async_script.src = "../resources/empty.js?dynamicNonAsync";
      non_async_script.async = false;
      document.head.appendChild(non_async_script);

      // Add a dynamic explicitly async script
      const async_script = document.createElement("script");
      async_script.src = "../resources/empty.js?dynamicAsync";
      async_script.async = true;
      document.head.appendChild(async_script);
    })();
  `);

  // Wait for traces to show up.
  for (let i = 0; i < numberOfURLs; ++i) {
    await dp.Network.onceRequestWillBeSent();
  }

  const events = await tracingHelper.stopTracing();
  const requestEvents = events.filter(e => e.name == "ResourceSendRequest");

  const resources = new Map();
  for (let e of requestEvents) {
    const data = e['args']['data'];
    const url_list = data['url'].split('/');
    const url = url_list[url_list.length - 1];
    if (url.includes("js")) {
      resources.set(url, data['renderBlocking']);
    }
  }
  for (const resource of Array.from(resources.keys()).sort())
    testRunner.log(`${resource}: ${resources.get(resource)}`);
  testRunner.completeTest();
})
