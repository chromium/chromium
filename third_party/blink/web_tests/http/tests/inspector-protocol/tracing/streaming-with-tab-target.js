(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget('Tests IO streams are available in the tab target.');

  const childTargetManager = new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const primarySession =
    childTargetManager.findAttachedSessionPrimaryMainFrame();
  await primarySession.navigate(testRunner.url('../resources/inspector-protocol-page.html'));

  const TracingHelper = await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, tabTargetSession);
  await tracingHelper.startTracingAndSaveAsStream();

  const streamHandle = await tracingHelper.stopTracingAndReturnStream();
  const data = await tracingHelper.retrieveStream(streamHandle, null, null);
  testRunner.log("Has tracing data: " + Boolean(data));
  testRunner.completeTest();
});
