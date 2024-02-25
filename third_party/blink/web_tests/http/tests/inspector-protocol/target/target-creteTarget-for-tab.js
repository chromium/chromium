(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      `Tests that browser.Target.createTarget() creates a tab target when forTab is set.`);

  const target = testRunner.browserP().Target;
  await target.setDiscoverTargets({discover: true});
  await target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const response = await target.attachToBrowserTarget();

  const newBrowserSession =
      new TestRunner.Session(testRunner, response.result.sessionId);
  const newUrl = testRunner.url('../resources/test-page.html');
  const {result} = await newBrowserSession.protocol.Target.createTarget({
                     url: newUrl,
                     forTab: true
                   });
  target.attachToTarget({targetId: result.targetId, flatten: true});

  const attachedEvent = await target.onceAttachedToTarget();
  testRunner.log(attachedEvent, 'Attached to the tab target: ');

  await newBrowserSession.disconnect();
  testRunner.completeTest();
})
