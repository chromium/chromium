(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that enabling CPU throttling in multiple pages does not crash.');

  const page1 = await testRunner.createPage();
  const session1 = await page1.createSession();
  await session1.protocol.Target.setDiscoverTargets({discover: true});

  session1.evaluate(`window.open('about:blank') && true`);

  const response = await session1.protocol.Target.onceTargetCreated();
  const session2 =
      await session1.attachChild(response.params.targetInfo.targetId);

  await session1.protocol.Emulation.setCPUThrottlingRate({rate: 2.0});
  await session2.protocol.Emulation.setCPUThrottlingRate({rate: 3.0});

  testRunner.completeTest();
})
