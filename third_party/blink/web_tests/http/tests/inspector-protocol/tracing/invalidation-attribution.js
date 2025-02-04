(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp, page} = await testRunner.startBlank(
      'Tests that we can attribute style invalidations to CSS selectors');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  let tracingHelper = new TracingHelper(testRunner, session);
  let timeline;

  await tracingHelper.startTracing(
    'blink,blink_style,disabled-by-default-devtools.timeline.invalidationTracking');

  await dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/before-after-pseudos.html'
  });

  // Wait for initial paint.
  await session.evaluateAsync(`
    new Promise((res) => {
      (new PerformanceObserver(res)).observe({entryTypes: ['largest-contentful-paint']});
    })`);

  // Run some script and wait for next update.
  await session.evaluateAsync(`
      (function() {
        const d = document.querySelector("div");
        d.classList.toggle("changed");
        return new Promise(f => testRunner.updateAllLifecyclePhasesAndCompositeThen(f));
      })();
    `);

  await tracingHelper.stopTracing(/.*/);

  timeline = tracingHelper.filterEvents((event) => {
    if (event.name == "StyleInvalidatorInvalidationTracking"
        || event.name == "Document::updateStyle"
        || event.name == "StyleResolver::ResolveStyle") {
      return true;
    }
    if (event.name == "StyleRecalcInvalidationTracking") {
      const reason = event.args?.data?.reason;
      return reason == "Node was inserted into tree" ||
        reason == "Related style rule";
    }
    return false;
  });

  for (let i = 0; i < timeline.length; i++) {
    const event = timeline[i];
    testRunner.log(event.name);
    if (event.name == "StyleInvalidatorInvalidationTracking") {
      testRunner.log('  nodeId: ' + event.args?.data?.nodeId);
      testRunner.log('  reason: ' + event.args?.data?.reason);
      testRunner.log('  selectorCount: ' + event.args?.data?.selectorCount);
      testRunner.log('  selectors[0]: ' + event.args?.data?.selectors[0]);
    } else if (event.name == "StyleRecalcInvalidationTracking") {
      testRunner.log('  nodeId: ' + event.args?.data?.nodeId);
      testRunner.log('  reason: ' + event.args?.data?.reason);
      testRunner.log('  subtree: ' + event.args?.data?.subtree);
    } else if (event.name == "Document::updateStyle") {
      testRunner.log('  elementsStyled: ' + event.args?.counters?.elementsStyled);
      testRunner.log('  pseudoElementsStyled: ' + event.args?.counters?.pseudoElementsStyled);
    } else if (event.name == "StyleResolver::ResolveStyle") {
      testRunner.log('  nodeId: ' + event.args?.data?.nodeId);
      testRunner.log('  parentNodeId: ' + event.args?.data?.parentNodeId);
      testRunner.log('  pseudoId: ' + event.args?.data?.pseudoId);
    }
  }

  testRunner.completeTest();
})
