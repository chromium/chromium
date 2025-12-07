(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp, page} = await testRunner.startHTML(
      `<div>
         <img>
       </div>
       <div>
         text
       </div>`,
      'Tests the data of invalidation tracking trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;
  let tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing(
      'disabled-by-default-devtools.timeline.invalidationTracking,devtools.timeline,disabled-by-default-devtools.timeline.stack');

  // Wait for trace events.
  const traceEventsPromise = session.evaluateAsync(`
      new Promise((res) => {
        (new PerformanceObserver(res)).observe({entryTypes: ['layout-shift']});
      })`);
  await session.evaluate(`
      function invalidateStyle() {
        const img = document.querySelector('img');
        img.setAttribute('src', './resources/big-image.png');
        const newStyle = document.createElement('style');
        newStyle.textContent = \`
      div {
        margin: 1px;
      }
      \`;
        document.head.appendChild(newStyle);
        testRunner.setAnimationRequiresRaster(true);
    }
    function forceLayout() {
        const img = document.querySelector('img');
        const unused = img.offsetHeight;
    }

    function performActions() {
        invalidateStyle();
        forceLayout();
    }

    performActions();`);

  await traceEventsPromise;

  await tracingHelper.stopTracing(
      /(disabled-by-default-)?devtools\.timeline(\.invalidationTracking|stack)?/);

  const styleRecalcInvalidationTracking = tracingHelper.findEvents(
      'StyleRecalcInvalidationTracking', Phase.INSTANT);
  const styleResolverResolveStyle =
      tracingHelper.findEvents('StyleResolver::ResolveStyle', Phase.INSTANT);
  const layoutInvalidationTracking =
      tracingHelper.findEvents('LayoutInvalidationTracking', Phase.INSTANT)[0];
  const layout = tracingHelper.findEvents(
      'Layout', Phase.COMPLETE, e => e.args?.beginData?.stackTrace);

  testRunner.log('StyleRecalcInvalidationTracking');
  tracingHelper.logEventShape(styleRecalcInvalidationTracking);

  testRunner.log('LayoutInvalidationTracking');
  tracingHelper.logEventShape(layoutInvalidationTracking);

  testRunner.log('Number of StyleResolver::ResolveStyle events:');
  testRunner.log(styleResolverResolveStyle.length);

  testRunner.log('Style recalc initiator:');
  testRunner.log(
      styleRecalcInvalidationTracking[0]
          .args?.data?.stackTrace[0]
          .functionName);

  testRunner.log('Layout initiator:');
  testRunner.log(layout[0]?.args?.beginData?.stackTrace[0].functionName);

  testRunner.completeTest();
})
