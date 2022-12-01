(async function(testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the data of rendering trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  await dp.Page.enable();
  await tracingHelper.startTracing(
      'disabled-by-default-devtools.timeline,devtools.timeline,v8.execute');
  dp.Page.navigate({
    url:
        'http://127.0.0.1:8000/inspector-protocol/resources/rendering-exercise.html'
  });

  // Wait for the DOM to be interactive.
  await dp.Page.onceLoadEventFired();

  // Dispatch a click and a scroll events.
  await dp.Input.dispatchMouseEvent({
    type: 'mousePressed',
    button: 'left',
    buttons: 0,
    clickCount: 1,
    x: 150,
    y: 150
  });

  await dp.Input.dispatchMouseEvent(
      {type: 'mouseWheel', x: 100, y: 200, deltaX: 50, deltaY: 70});

  // Wait for trace events.
  await session.evaluateAsync(`window.__intersectionPromise`);
  await session.evaluateAsync(`window.__blockingHandlerPromise`);

  await tracingHelper.stopTracing(
      /(disabled-by-default-)?devtools\.timeline|v8.execute/);

  const animationEventBegin = tracingHelper.findEvent('Animation', 'b');
  const hitTest = tracingHelper.findEvent('HitTest', 'X');
  const scheduleStyleRecalculation =
      tracingHelper.findEvent('ScheduleStyleRecalculation', 'I');
  const updateLayoutTree = tracingHelper.findEvent('UpdateLayoutTree', 'X');
  const invalidateLayout = tracingHelper.findEvent('InvalidateLayout', 'I');
  const updateLayer = tracingHelper.findEvent('UpdateLayer', 'X');
  const paintImage = tracingHelper.findEvent('PaintImage', 'X');
  const prePaint = tracingHelper.findEvent('PrePaint', 'X');
  const rasterTask = tracingHelper.findEvent('RasterTask', 'X');
  const scrollLayer = tracingHelper.findEvent('ScrollLayer', 'X');
  const computeIntersections = tracingHelper.findEvent(
      'IntersectionObserverController::computeIntersections', 'X');
  const parseHTML = tracingHelper.findEvent('ParseHTML', 'X');
  const parseAuthorStyleSheet =
      tracingHelper.findEvent('ParseAuthorStyleSheet', 'X');
  const layout = tracingHelper.findEvent('Layout', 'X');
  const runMicrotasks = tracingHelper.findEvent('RunMicrotasks', 'X');
  const functionCall = tracingHelper.findEvent('FunctionCall', 'X');

  testRunner.log('Got a HitTest event:');
  tracingHelper.logEventShape(hitTest, ['move']);

  testRunner.log('Got an Animation event');
  tracingHelper.logEventShape(animationEventBegin);

  testRunner.log('Got a ScheduleStyleRecalculation Event');
  tracingHelper.logEventShape(scheduleStyleRecalculation);

  testRunner.log('Got an UpdateLayoutTree');
  tracingHelper.logEventShape(updateLayoutTree);

  testRunner.log('Got an InvalidateLayout');
  tracingHelper.logEventShape(invalidateLayout);

  testRunner.log('Got an UpdateLayer');
  tracingHelper.logEventShape(updateLayer);

  testRunner.log('Got an PrePaint');
  tracingHelper.logEventShape(prePaint);

  testRunner.log('Got a PaintImage');
  tracingHelper.logEventShape(paintImage);

  testRunner.log('Got a RasterTask');
  tracingHelper.logEventShape(rasterTask);

  testRunner.log('Got a ScrollLayer');
  tracingHelper.logEventShape(scrollLayer);

  testRunner.log('Got a ComputeIntersections');
  tracingHelper.logEventShape(computeIntersections);

  testRunner.log('Got a ParseHTML');
  tracingHelper.logEventShape(parseHTML);

  testRunner.log('Got a ParseAuthorStyleSheet');
  tracingHelper.logEventShape(parseAuthorStyleSheet);

  testRunner.log('Got a Layout');
  tracingHelper.logEventShape(layout, ['quads']);

  testRunner.log('Got a RunMicrotasks');
  tracingHelper.logEventShape(runMicrotasks);

  testRunner.log('Got a FunctionCall');
  tracingHelper.logEventShape(functionCall);

  testRunner.completeTest();
})
