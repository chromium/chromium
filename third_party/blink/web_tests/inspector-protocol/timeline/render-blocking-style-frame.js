(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // The number includes the frame, the 5 CSS files it loads directly, and the
  // one imported by them.
  const numberOfURLs = 14;

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
      const frame = document.createElement("iframe");
      frame.src = "../resources/render-blocking-frame.html";
      document.body.appendChild(frame);
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
