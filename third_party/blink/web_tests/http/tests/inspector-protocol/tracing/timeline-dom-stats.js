(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id="parent">
      <div class="empty"></div>
    </div>
  `, 'Tests trace events for dom stats.');

  async function performActions() {
    const parent = document.getElementById('parent');
    const child = document.createElement('div');
    child.id = 'child';
    parent.append(child);

    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);

    for (let i = 0; i < 50; ++i) {
      const newChild = document.createElement('div');
      newChild.id = 'new-' + i;
      parent.append(newChild);
    }

    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);

    const shadow = child.attachShadow({mode: 'open'});
    const shadowChild = document.createElement('div');
    shadowChild.id = 'shadow-child';
    const shadowLeaf = document.createElement('div');
    shadowLeaf.id = 'shadow-leaf';
    shadowChild.append(shadowLeaf);
    shadow.append(shadowChild);
    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
  }

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await tracingHelper.invokeAsyncWithTracing(performActions);

  const domStatsEvents = tracingHelper.findEvents('DOMStats', 'I');
  testRunner.log('First DOM stats');
  tracingHelper.logEventShape(domStatsEvents.at(0), [], ['numChildren', 'depth', 'totalElements', 'nodeName']);
  testRunner.log('Last DOM stats');
  tracingHelper.logEventShape(domStatsEvents.at(-1), [], ['numChildren', 'depth', 'totalElements', 'nodeName']);

  await tracingHelper.invokeAsyncWithTracing(async () => {
    document.body.innerHTML = '';

    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
  });
  const domStatsEventsEmptyBody = tracingHelper.findEvents('DOMStats', 'I');
  testRunner.log('DOM stats with empty body element');
  tracingHelper.logEventShape(domStatsEventsEmptyBody.at(-1), [], ['numChildren', 'depth', 'totalElements', 'nodeName']);

  await tracingHelper.invokeAsyncWithTracing(async () => {
    document.body.remove();

    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
  });
  const domStatsEventsNoBody = tracingHelper.findEvents('DOMStats', 'I');
  testRunner.log('DOM stats with no body element');
  tracingHelper.logEventShape(domStatsEventsNoBody.at(-1), [], ['numChildren', 'depth', 'totalElements', 'nodeName']);

  await tracingHelper.invokeAsyncWithTracing(async () => {
    document.documentElement.remove();

    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
  });
  const domStatsEventsNoDoc = tracingHelper.findEvents('DOMStats', 'I');
  testRunner.log('DOM stats with no document element');
  tracingHelper.logEventShape(domStatsEventsNoDoc.at(-1), [], ['numChildren', 'depth', 'totalElements', 'nodeName']);
  testRunner.completeTest();
})
