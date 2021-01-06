(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Basic test for LargestContentfulPaint support in PerformanceTimeline');
  const unstableFields = ['time', 'renderTime', 'loadTime', 'frameId'];

  const events = [];
  const startTime = Date.now();
  await dp.PerformanceTimeline.enable({eventTypes: ['largest-contentful-paint']});

  dp.PerformanceTimeline.onTimelineEventAdded(event => events.push(event.params.event));
  session.navigate(testRunner.url('resources/lcp.html'));
  await dp.PerformanceTimeline.onceTimelineEventAdded();

  session.evaluate(`
    const img = document.createElement("img");
    img.id = "image";
    img.src = "${testRunner.url('../../images/resources/green-256x256.jpg')}";
    document.body.appendChild(img);
  `);
  await dp.PerformanceTimeline.onceTimelineEventAdded();

  const endTime = Date.now();

  for (const event of events) {
    checkTime(event.time);
    if (event.lcpDetails.renderTime)
      checkTime(event.lcpDetails.renderTime);
    if (event.lcpDetails.loadTime)
      checkTime(event.lcpDetails.loadTime);
    await patchFields(event.lcpDetails);
  }

  testRunner.log(events, null, unstableFields);
  testRunner.completeTest();

  function checkTime(time) {
    const timeInMs = time * 1000;
    if (timeInMs < startTime || timeInMs > endTime)
      testRunner.log(`FAIL: event time out of bounds, expect ${startTime} <= ${timeInMs} <= ${endTime}`);
  }

  async function patchFields(object) {
    if (object.url)
      object.url = testRunner.trimURL(object.url);
    if (object.nodeId)
      object.nodeId = await describeNode(object.nodeId);
  }

  async function describeNode(nodeId) {
    const response = await dp.DOM.resolveNode({backendNodeId: nodeId});
    return response.result && response.result.object.description ?
        `<${response.result.object.description}>` : '<invalid id>';
  }
})
