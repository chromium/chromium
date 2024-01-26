(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, page} = await testRunner.startHTML(`
    <script>setTimeout(() => { while (true) {} }, 0)</script>
  `,'Tests that Target.closeTarget works for unresponsive renderer');

  const browserSession = await testRunner.attachFullBrowserSession();
  const bp = browserSession.protocol;
  await bp.Target.setDiscoverTargets({discover: true});

  const event = bp.Target.onceTargetDestroyed();
  testRunner.log("closed:");
  testRunner.log(await dp.Target.closeTarget({
    targetId: page.targetId(),
  }));
  testRunner.log("destroyed:");
  testRunner.log(await event);

  testRunner.completeTest();
})
