(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests the data of user marks trace events');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);
  const Phase = TracingHelper.Phase;
  await dp.Page.enable();
  await tracingHelper.startTracing(
      'devtools.timeline,blink.console,blink.user_timing');
  dp.Page.navigate({
    url: 'http://127.0.0.1:8000/inspector-protocol/resources/user-marks.html'
  });

  // Wait for trace events.
  await dp.Page.onceLoadEventFired();
  await session.evaluateAsync('dispatchIdleCallback();');
  await session.evaluateAsync('dispatchConsoleTimings();');
  await session.evaluateAsync('dispatchConsoleTimeStamps();');
  await session.evaluateAsync('dispatchConsoleTimeStampsWithOptionalData();');
  await session.evaluateAsync('dispatchConsoleTimeStampsWithUnexpectedObjectParam();');
  await session.evaluateAsync('dispatchConsoleTimeStampsForWorkerThread();');
  await session.evaluateAsync('dispatchUserTimings();');
  await session.evaluateAsync('dispatchAnimationFrame();');
  await session.evaluateAsync('dispatchTimer();');
  await session.evaluateAsync('window.__requestIdleCallbackPromise');

  const allEvents = await tracingHelper.stopTracing(
      /devtools\.timeline|blink\.console|blink\.user_timing/);

  const timeStampWithLabels = allEvents.find(event => event.name === 'TimeStamp' && event.args?.data?.name === "Timestamp with labels");
  const timeStampWithOptionalData = allEvents.find(event => event.name === 'TimeStamp' && event.args?.data?.name === "Timestamp with optional data");
  const timeStampsWithUnexpectedObjectParam = allEvents.find(event => event.name === 'TimeStamp' && event.args?.data?.name === "Timestamp with unexpected object");
  const timeStampsForWorkerThread = allEvents.find(event => event.name === 'TimeStamp' && event.args?.data?.name === "Timestamp for worker thread");
  const markReference = tracingHelper.findEvent('Timestamp reference', Phase.INSTANT);
  const timeStampWithNumbers = allEvents.find(event => event.name === 'TimeStamp' && event.args?.data?.name === "Timestamp with numeric values");
  const consoleTimeEvents =
      allEvents.filter(event => event.name === 'console time');

  const performanceMark = tracingHelper.findEvent('startMark', Phase.INSTANT);
  const userTimings = allEvents.filter(event => event.name === 'user timing');
  const performanceMeasureTraces = allEvents.filter(event => event.name === 'UserTiming::Measure');

  const timerRemove = tracingHelper.findEvent('TimerRemove', Phase.INSTANT);
  const timerId = timerRemove.args.data.timerId;
  const timerInstall = allEvents.find(
      event =>
          event.name === 'TimerInstall' && event.args.data.timerId === timerId);
  const timerFire = allEvents.find(
      event =>
          event.name === 'TimerFire' && event.args.data.timerId === timerId);

  const cancelIdleCallback =
      tracingHelper.findEvent('CancelIdleCallback', Phase.INSTANT);
  const idleCallbackId = cancelIdleCallback.args.data.id;
  const requestIdleCallback = allEvents.find(
      event => event.name === 'RequestIdleCallback' &&
          event.args.data.id === idleCallbackId);
  const fireIdleCallback = allEvents.find(
      event => event.name === 'FireIdleCallback' &&
          event.args.data.id === idleCallbackId);

  const cancelAnimationFrame =
      tracingHelper.findEvent('CancelAnimationFrame', Phase.INSTANT);
  const animationFrameId = cancelAnimationFrame.args.data.id;
  const requestAnimationFrame = allEvents.find(
      event => event.name === 'RequestAnimationFrame' &&
          event.args.data.id === animationFrameId);
  const fireAnimationFrame = allEvents.find(
      event => event.name === 'FireAnimationFrame' &&
          event.args.data.id === animationFrameId);


  testRunner.log('Got a TimeStamp event with labeled start and end:');
  tracingHelper.logEventShape(timeStampWithLabels);

  testRunner.log('Got a TimeStamp event with optional data:');
  tracingHelper.logEventShape(timeStampWithOptionalData);

  testRunner.log('Got a TimeStamp event with unexpected object param:');
  tracingHelper.logEventShape(timeStampsWithUnexpectedObjectParam);

  testRunner.log('Got a TimeStamp event for worker thread:');
  tracingHelper.logEventShape(timeStampsForWorkerThread);

  testRunner.log('Got a TimeStamp event with numeric start and end:');
  tracingHelper.logEventShape(timeStampWithNumbers);

  if (markReference.ts === timeStampWithNumbers.args?.data?.start && markReference.ts + 100_000 === timeStampWithNumbers.args?.data?.end) {
    testRunner.log('TimeStamp numeric start and end are correct.');
  }

  testRunner.log('Got ConsoleTime events:');
  tracingHelper.logEventShape(consoleTimeEvents[0]);
  testRunner.log(`Phase of begin event: ${consoleTimeEvents[0].ph}`);
  testRunner.log(`Phase of end event: ${consoleTimeEvents[1].ph}`);
  if (consoleTimeEvents[0].id2 === consoleTimeEvents[1].id2) {
    testRunner.log('ConsoleTime event ids are equal.');
  }

  testRunner.log('Got a performance mark event:');
  tracingHelper.logEventShape(performanceMark);

  testRunner.log('Got performance measure event:');
  tracingHelper.logEventShape(userTimings[0]);
  testRunner.log(`Phase of begin event: ${userTimings[0].ph}`);
  testRunner.log(`Phase of end event: ${userTimings[1].ph}`);
  if (userTimings[0].id2.local === userTimings[1].id2.local) {
    testRunner.log('user timing event ids are equal.');
  }
  testRunner.log('Got trace for performance measure call:');
  tracingHelper.logEventShape(performanceMeasureTraces[0]);
  if (performanceMeasureTraces[0].args.traceId === userTimings[0].args.traceId) {
      testRunner.log('Performance measure trace matches user timing event by trace id.');
  }

  testRunner.log('Got a TimerInstall event:');
  tracingHelper.logEventShape(timerInstall);

  testRunner.log('Got a TimerFire event:');
  tracingHelper.logEventShape(timerFire);

  testRunner.log('Got a TimerRemove event:');
  tracingHelper.logEventShape(timerRemove);

  testRunner.log('Got a RequestIdleCallback event:');
  tracingHelper.logEventShape(requestIdleCallback);

  testRunner.log('Got a CancelIdleCallback event:');
  tracingHelper.logEventShape(cancelIdleCallback);

  testRunner.log('Got a FireIdleCallback event:');
  tracingHelper.logEventShape(fireIdleCallback);

  testRunner.log('Got a RequestAnimationFrame event:');
  tracingHelper.logEventShape(requestAnimationFrame);

  testRunner.log('Got a CancelAnimationFrame event:');
  tracingHelper.logEventShape(cancelAnimationFrame);

  testRunner.log('Got a FireAnimationFrame event:');
  tracingHelper.logEventShape(fireAnimationFrame);

  testRunner.completeTest();
})
