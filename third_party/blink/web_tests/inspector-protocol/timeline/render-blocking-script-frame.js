(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // The number includes the frame, and the 9 JS files that it loads directly.
  const numberOfURLs = 11;

  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests render-blocking status of HTML based script in traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing();
  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      const frame = document.createElement("iframe");
      frame.src = "../resources/render-blocking-frame-script.html";
      document.body.appendChild(frame);
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
