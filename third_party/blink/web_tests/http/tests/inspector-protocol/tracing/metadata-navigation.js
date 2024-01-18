(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests metadata trace events during frame navigation');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, session);

  await dp.Page.enable();
  await dp.Page.navigate({url: 'about:blank'});

  await tracingHelper.startTracing();

  await dp.Page.navigate({ url: testRunner.url('../resources/test-page.html')});

  await tracingHelper.stopTracing();

  const frameCommittedInBrowserEvents =
      tracingHelper.findEvents('FrameCommittedInBrowser', TracingHelper.Phase.INSTANT);

  testRunner.log(frameCommittedInBrowserEvents.map(e => e.args.data),
      'FrameCommittedInBrowser ',
      ['frame', 'processId']);
  testRunner.completeTest();
});
