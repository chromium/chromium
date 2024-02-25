(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that setAutoAttach honors updated waitForDebuggerOnStart.`);

  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});

  const dp2 = (await page.createSession()).protocol;
  await dp2.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  session.navigate(testRunner.url('../resources/site_per_process_main.html'));
  const attached = await Promise.all([
    dp.Target.onceAttachedToTarget(),
    dp2.Target.onceAttachedToTarget()
  ]);
  testRunner.log(attached);
  testRunner.completeTest();
})
