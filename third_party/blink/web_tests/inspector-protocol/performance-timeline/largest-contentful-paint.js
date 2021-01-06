(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Basic test for LargestContentfulPaint support in PerformanceTimeline');
  const unstableFields = ['frameId'];

  const events = [];
  const startTime = Date.now();
  await dp.PerformanceTimeline.enable({eventTypes: ['largest-contentful-paint']});

  const TestHelper = await testRunner.loadScript('resources/performance-timeline-test.js');
  const testHelper = new TestHelper(dp);

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
    testHelper.patchTimes(startTime, endTime, event, ['time']);
    testHelper.patchTimes(startTime, endTime, event.lcpDetails, ['renderTime', 'loadTime']);
    await patchFields(event.lcpDetails);
  }

  testRunner.log(events, null, unstableFields);
  testRunner.completeTest();

  async function patchFields(object) {
    if (object.url)
      object.url = testRunner.trimURL(object.url);
    if (object.nodeId)
      object.nodeId = await testHelper.describeNode(object.nodeId);
  }
})
