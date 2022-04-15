(async function(testRunner) {
  const numberOfURLs = 12;

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests render blocking status in preload traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add a dynamic preload with DOM API
      let link = document.createElement("link");
      link.href = "../resources/style.css?dynamic";
      link.rel = "preload";
      link.as = "style";
      document.head.appendChild(link);

      // Add a dynamic render-blocking preload DOM API
      link = document.createElement("link");
      link.href = "../resources/style.css?dynamicBlocking";
      link.rel = "preload";
      link.as = "style";
      link.blocking = "render"
      document.head.appendChild(link);

      // Add a dynamic modulepreload with DOM API
      link = document.createElement("link");
      link.href = "../resources/empty.js?dynamic";
      link.rel = "modulepreload";
      document.head.appendChild(link);

      // Add a dynamic render-blocking modulepreload with DOM API
      link = document.createElement("link");
      link.href = "../resources/empty.js?dynamicBlocking";
      link.rel = "modulepreload";
      link.blocking = "render";
      document.head.appendChild(link);

      // Add dynamic preloads with document.write()
      document.write("<link rel=preload as=style href='../resources/style.css?dynamicDocWrite'>");
      document.write("<link rel=preload as=style blocking=render href='../resources/style.css?dynamicDocWriteBlocking'>");

      // Add dynamic modulepreloads with document.write()
      document.write("<link rel=modulepreload href='../resources/empty.js?dynamicDocWrite'>");
      document.write("<link rel=modulepreload blocking=render href='../resources/empty.js?dynamicDocWriteBlocking'>");

      // Add dynamic preloads with innerHTML
      document.head.innerHTML += "<link rel=preload as=style href='../resources/style.css?dynamicInnerHTML'>";
      document.head.innerHTML += "<link rel=preload as=style blocking=render href='../resources/style.css?dynamicInnerHTMLBlocking'>";

      // Add dynamic modulepreloads with innerHTML
      document.head.innerHTML += "<link rel=preload as=style href='../resources/empty.js?dynamicInnerHTML'>";
      document.head.innerHTML += "<link rel=preload as=style blocking=render href='../resources/empty.js?dynamicInnerHTMLBlocking'>";
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
    if (url.includes("css") || url.includes("js")) {
      resources.set(url, data['renderBlocking']);
    }
  }
  for (const resource of Array.from(resources.keys()).sort())
    testRunner.log(`${resource}: ${resources.get(resource)}`);
  testRunner.completeTest();
})
