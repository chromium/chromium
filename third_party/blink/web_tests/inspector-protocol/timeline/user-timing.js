(async function(testRunner) {

  // Test traces
  var {page, session, dp} = await testRunner.startHTML(`
      <head></head>
      <body>
      </body>
  `, 'Tests render blocking status in script traces.');

  var TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  var tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.startTracing("blink.user_timing");
  dp.Network.enable();
  await session.evaluateAsync(`
    (async function performActions() {
        const now = performance.now();
        performance.mark("now", {detail: "now", startTime: now});
        performance.mark("first");
        performance.mark("second");
        await new Promise(resolve => {
            setTimeout(resolve, 100);
        });
        performance.mark("third");
        performance.measure("now to first", "now", "first");
        performance.measure("first to second", "first", "second");
        performance.measure("first to third", "first", "third");
    })();
  `);

  const events = await tracingHelper.stopTracing(/blink.user_timing/);
  const sorted_events = events.sort((a, b) => {
    return (a["ts"] - b["ts"]);
  });

  for (let e of sorted_events) {
    testRunner.log(e["name"]);
  }

  testRunner.completeTest();
})
