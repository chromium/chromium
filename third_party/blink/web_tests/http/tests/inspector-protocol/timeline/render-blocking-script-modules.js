(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // The number includes the two directly imported modules and the two they then
  // import (each).
  const numberOfURLs = 6;

  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests render blocking status of module script in traces.');

  // Start tracing
  await session.protocol.Tracing.start({ "categories": "-*,disabled-by-default-devtools.timeline,devtools.timeline", "type": "", "options": "" });

  dp.Network.enable();
  session.evaluate(`
    (function performActions() {
      // Add a module that imports more modules
      const script = document.createElement("script");
      script.src = "../resources/es_module.php?url=importer.js";
      script.type = "module";
      document.head.appendChild(script);

      // Add an async module that imports more modules
      const async_script = document.createElement("script");
      async_script.src = "../resources/es_module.php?url=importer_async.js";
      async_script.type = "module";
      async_script.async = true;
      document.head.appendChild(async_script);
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
