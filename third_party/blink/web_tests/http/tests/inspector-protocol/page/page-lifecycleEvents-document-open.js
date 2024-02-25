(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that Page.lifecycleEvent is issued for important events.`);

  await dp.Page.enable();
  await dp.Page.setLifecycleEventsEnabled({ enabled: true });

  const expectedEvents = new Set([
    'init',
    'load',
    'DOMContentLoaded',
    'networkAlmostIdle',
    'networkIdle',
    'InteractiveTime',
  ]);

  dp.Page.onLifecycleEvent(event => {
    // Filter out firstMeaningfulPaint and friends.
    if (event.params.name.startsWith('first'))
      return;
    if (!expectedEvents.delete(event.params.name)) {
      testRunner.log(`FAIL: unexpected event name: ${event.params.name}`);
    }
    if (expectedEvents.size === 0) {
      testRunner.completeTest();
    }
  });

  var response = await session.evaluate(`
    document.open();
    document.write('Hello, world');
    document.close();
  `);
})
