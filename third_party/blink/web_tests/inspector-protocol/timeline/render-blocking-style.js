(async function(testRunner) {
  // The number includes the 2 imported CSS files
  const numberOfURLs = 9;

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests various style traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add a dynamic style with DOM API
      const link = document.createElement("link");
      link.href = "../resources/style.css?dynamicDOM";
      link.rel = "stylesheet";
      document.head.appendChild(link);

      // Add a dynamic style with document.write
      document.write("<link rel=stylesheet href='../resources/style.css?dynamicDocWrite'>");
      document.write("<link rel=stylesheet href='../resources/style.css?dynamicDocWritePrint' media=print>");

      // Add a dynamic style with innerHTML
      document.head.innerHTML += "<link rel=stylesheet href='../resources/style.css?dynamicInnerHTML'>";
      document.head.innerHTML += "<link rel=stylesheet href='../resources/style.css?dynamicInnerHTMLPrint' media=print>";

      // Add a CSS importer
      document.write("<link rel=stylesheet href='../resources/importer.css'>");
      document.write("<link rel=stylesheet href='../resources/importer_print.css' media=print>");
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
    resources.set(url, data['renderBlocking']);
  }
  for (const resource of Array.from(resources.keys()).sort())
    testRunner.log(`${resource}: ${resources.get(resource)}`);
  testRunner.completeTest();
})
