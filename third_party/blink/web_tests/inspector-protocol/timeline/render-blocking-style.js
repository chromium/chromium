(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // The number includes the imported CSS files
  const numberOfURLs = 18;

  // Test traces
  var {page, session, dp} = await testRunner.startBlank('Tests various style traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  await page.navigate('../resources/disabled-stylesheet.html');

  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add a dynamic style with DOM API
      let link = document.createElement("link");
      link.href = "../resources/style.css?dynamicDOM";
      link.rel = "stylesheet";
      document.head.appendChild(link);

      // Add a dynamic render-blocking style with DOM API
      link = document.createElement("link");
      link.href = "../resources/style.css?dynamicDOMBlocking";
      link.rel = "stylesheet";
      link.blocking = "render";
      document.head.appendChild(link);

      // Add a style preload with DOM API
      link = document.createElement("link");
      link.href = "../resources/style.css?preload";
      link.rel = "preload";
      link.as = "style";
      document.head.appendChild(link);

      // Add a style preload with DOM API to be used later
      link = document.createElement("link");
      link.href = "../resources/style.css?preload_used";
      link.rel = "preload";
      link.as = "style";
      document.head.appendChild(link);

      // Use the preload
      link = document.createElement("link");
      link.href = "../resources/style.css?preload_used";
      link.rel = "stylesheet";
      document.head.appendChild(link);

      // Add a dynamic style with document.write
      document.write("<link rel=stylesheet href='../resources/style.css?dynamicDocWrite'>");
      document.write("<link rel=stylesheet href='../resources/style.css?dynamicDocWritePrint' media=print>");
      document.write("<link rel=stylesheet blocking=render href='../resources/style.css?dynamicDocWriteBlocking'>");

      // Add a dynamic style with innerHTML
      document.head.innerHTML += "<link rel=stylesheet href='../resources/style.css?dynamicInnerHTML'>";
      document.head.innerHTML += "<link rel=stylesheet href='../resources/style.css?dynamicInnerHTMLPrint' media=print>";
      document.head.innerHTML += "<link rel=stylesheet blocking=render href='../resources/style.css?dynamicInnerHTMLBlocking'>";

      // Add a CSS importer
      document.write("<link rel=stylesheet href='../resources/importer.css'>");
      document.write("<link rel=stylesheet href='../resources/importer_print.css' media=print>");

      // Add an inline CSS importer
      document.write("<style>@import url('../resources/style.css?inlineImported')</style>");
      document.write("<style media=print>@import url('../resources/style.css?inlineImportedPrint')</style>");

      // Add a dynamic inline CSS importer
      let style = document.createElement("style");
      style.textContent = "@import url('../ressources/style.css?dynamicInlineImported')";
      document.head.appendChild(style);

      // Add a dynamic render-blocking inline CSS importer
      style = document.createElement("style");
      style.textContent = "@import url('../ressources/style.css?dynamicInlineImportedBlocking')";
      style.blocking = "render";
      document.head.appendChild(style);
    })();
  `);

  // Wait for traces to show up.
  for (let i = 0; i < numberOfURLs; ++i) {
    await dp.Network.onceRequestWillBeSent();
  }

  const events = await tracingHelper.stopTracing();
  const requestEvents = events.filter(e =>
      (e.name == "ResourceSendRequest" ||
       e.name == "PreloadRenderBlockingStatusChange"));

  const resources = new Map();
  for (let e of requestEvents) {
    const data = e['args']['data'];
    const url_list = data['url'].split('/');
    const url = url_list[url_list.length - 1];
    if (url.includes("css")) {
      const previousValue = resources.get(url);
      if (previousValue) {
        const descriptor = (previousValue[1] === data['requestId']) ?
          "identical" : "different";
        testRunner.log(`Previous value requestId is ${descriptor} to the ` +
          `current one`);
      }
      resources.set(url, [data['renderBlocking'], data['requestId']]);
    }
  }
  for (const resource of Array.from(resources.keys()).sort())
    testRunner.log(`${resource}: ${resources.get(resource)[0]}`);
  testRunner.completeTest();
})
