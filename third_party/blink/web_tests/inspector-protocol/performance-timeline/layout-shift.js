(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank('Basic test for LayoutShift support in PerformanceTimeline');
  const unstableFields = ['frameId'];

  const events = [];

  const TestHelper = await testRunner.loadScript('resources/performance-timeline-test.js');
  const testHelper = new TestHelper(dp);

  await dp.PerformanceTimeline.enable({eventTypes: ['layout-shift']});

  dp.PerformanceTimeline.onTimelineEventAdded(event => events.push(event.params.event));
  await session.navigate(testRunner.url('resources/layout-shift.html'));
  await session.evaluateAsync(`new Promise(resolve => requestAnimationFrame(resolve))`);

  session.evaluate(`
    document.getElementById('padding1').style.display = 'block';
  `);
  await dp.PerformanceTimeline.onceTimelineEventAdded();

  // Now make an input-induced layout shift to assure input-related fields
  // are properly populated.
  click(10, 150);

  await dp.PerformanceTimeline.onceTimelineEventAdded();

  for (const event of events) {
    testHelper.patchTimes(event, ['time']);
    testHelper.patchTimes(event.layoutShiftDetails, ['lastInputTime']);
    await patchFields(event.layoutShiftDetails);
  }

  testRunner.log(events, null, unstableFields);
  testRunner.completeTest();

  async function patchFields(object) {
    for (const source of object.sources) {
      if (source.nodeId)
        source.nodeId = await testHelper.describeNode(source.nodeId);
    }
  }

  async function click(x, y) {
    await dp.Input.dispatchMouseEvent({type: 'mouseMoved', button: 'left', buttons: 0, clickCount: 1, x, y });
    await dp.Input.dispatchMouseEvent({type: 'mousePressed', button: 'left', buttons: 0, clickCount: 1, x, y });
    await dp.Input.dispatchMouseEvent({type: 'mouseReleased', button: 'left', buttons: 1, clickCount: 1, x, y });
  }
})
