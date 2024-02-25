(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'https://devtools.oopif-a.test:8443/inspector-protocol/resources/coi-with-iframe.php',
      `Tests that the attachedToTarget message is dispatched to all attached sessions`);

  // This will force the iframe to navigate out-of-process.
  await session.evaluate(
    'document.getElementById(\'iframe\').src=\'https://devtools.oopif-b.test:8443/inspector-protocol/resources/corp-iframe.php\'');

  await dp.Target.enable();
  const session1Attached = dp.Target.onceAttachedToTarget().then(
    () => testRunner.log('session 1 attached'));
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  await session1Attached;

  const dp2 = (await page.createSession()).protocol;
  const session2Attached = dp2.Target.onceAttachedToTarget().then(
    () => testRunner.log('session 2 attached'));
  await dp2.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  await session2Attached;

  testRunner.completeTest();
})
