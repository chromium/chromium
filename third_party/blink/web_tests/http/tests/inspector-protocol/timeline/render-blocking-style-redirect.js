(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // The number includes the initial request and the redirected one that follows.
  const numberOfURLs = 2;

  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests redirected style traces.');

  // Start tracing
  await session.protocol.Tracing.start({ "categories": "-*,disabled-by-default-devtools.timeline,devtools.timeline", "type": "", "options": "" });

  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add a dynamic style with DOM API
      const link = document.createElement("link");
      link.href = "../resources/css-redirect.php";
      link.rel = "stylesheet";
      document.head.appendChild(link);
    })();
  `);

  // Wait for traces to show up.
  for (let i = 0; i < numberOfURLs; ++i) {
    await dp.Network.onceRequestWillBeSent();
  }

  let events = [];
  const collectEvents = reply => {
    events = events.concat(reply.params.value);
  }
  session.protocol.Tracing.onDataCollected(collectEvents);
  session.protocol.Tracing.end();
  await session.protocol.Tracing.onceTracingComplete();
  session.protocol.Tracing.offDataCollected(collectEvents);

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
