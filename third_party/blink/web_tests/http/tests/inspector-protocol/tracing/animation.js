(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {session, dp} = await testRunner.startBlank(
        'Tests the data of an Animation event');

    const TracingHelper =
        await testRunner.loadScript('../resources/tracing-test.js');
    const tracingHelper = new TracingHelper(testRunner, session);

    await dp.Page.enable();
    await dp.Animation.enable();

    await tracingHelper.startTracing('blink.animations,devtools.timeline,benchmark,rail');

    dp.Page.navigate(
        {url: 'http://127.0.0.1:8000/inspector-protocol/resources/animation.html'});

    // Wait for animation.
    await dp.Animation.onceAnimationStarted();

    const events = await tracingHelper.stopTracing(/blink\.animations|devtools\.timeline|benchmark|rail/);
    const animationEvents = events.filter(event => event.name && event.name === 'Animation' && event.ph !== 'n').sort((a, b) => a.ts - b.ts);
    for (const event of animationEvents) {
        tracingHelper.logEventShape(event, [], ['name', 'data', 'ph', 'state', 'compositeFailed', 'unsupportedProperties', 'displayName']);
    }

    testRunner.completeTest();
  });
