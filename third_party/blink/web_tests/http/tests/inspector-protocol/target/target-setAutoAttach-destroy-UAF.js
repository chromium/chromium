(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that iterator invalidation in AutoAttacherDestroyed is fixed.`);

  const browserTarget = testRunner.browserP().Target;

  const loadPromises = [];
  browserTarget.onAttachedToTarget(async event => {
    if (event.params.targetInfo.type === 'page' &&
        event.params.targetInfo.url.includes('empty.html')) {
      const s = new TestRunner.Session(testRunner, event.params.sessionId);
      s.protocol.Page.enable();
      loadPromises.push(s.protocol.Page.onceLoadEventFired());
    }
  });

  await browserTarget.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  testRunner.log('Enabled auto attach on browser target.');

  // Create 3 new targets. They will be deferred by RequestThrottle in the
  // browser target.
  await dp.Target.createTarget(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/empty.html?1'});
  await dp.Target.createTarget(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/empty.html?2'});
  await dp.Target.createTarget(
      {url: 'http://127.0.0.1:8000/inspector-protocol/resources/empty.html?3'});
  testRunner.log('Created 3 targets. Throttles are now deferred.');

  // Call setAutoAttach AGAIN on the browser target.
  // This will trigger RemoveClient -> AutoAttacherDestroyed on the Browser
  // target's TargetAutoAttacher. If unfixed, the loop will skip the second
  // throttle, leaving it deferred.
  await browserTarget.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  testRunner.log('Called setAutoAttach again on browser target.');

  // Now wait for all 3 targets to finish loading.
  // If unfixed, the second target will never load and this will timeout.
  await Promise.all(loadPromises);
  testRunner.log('All targets loaded successfully.');
  testRunner.completeTest();
})
