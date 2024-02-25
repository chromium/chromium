(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pageUrl =
      'http://127.0.0.1:8000/inspector-protocol/prerender/resources/inspector-protocol-page.html';
  const {tabTargetSession} = await testRunner.startBlankWithTabTarget(
      'Test that prerender page is included in the trace events');

  const tp = tabTargetSession.protocol;

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, tabTargetSession);

  const childTargetManager =
      new TestRunner.ChildTargetManager(testRunner, tabTargetSession);
  await childTargetManager.startAutoAttach();
  const primarySession =
      childTargetManager.findAttachedSessionPrimaryMainFrame();
  const dp = primarySession.protocol;

  primarySession.navigate(pageUrl);

  await dp.Preload.enable();
  await tracingHelper.startTracing();

  primarySession.navigate('../prerender/resources/simple-prerender.html');
  await dp.Preload.oncePrerenderStatusUpdated(e => e.params.status == 'Ready');
  const prerenderSession = childTargetManager.findAttachedSessionPrerender();
  const pp = prerenderSession.protocol;
  await Promise.all([pp.Preload.enable(), pp.Page.enable()]);
  primarySession.evaluate(`document.getElementById('link').click()`);

  await Promise.all([
    pp.Preload.oncePrerenderStatusUpdated(e => e.params.status === 'Success'),
    pp.Page.setLifecycleEventsEnabled({ enabled: true }),
    pp.Page.onceLifecycleEvent(event => event.params.name === 'load'),
  ]);

  const devtoolsEvents = await tracingHelper.stopTracing();
  const prerenderFrameCommitted =
      tracingHelper
          .findEvents('FrameCommittedInBrowser', TracingHelper.Phase.INSTANT)
          .find(
              e => e.args.data.url ===
                  'http://127.0.0.1:8000/inspector-protocol/prerender/resources/empty.html');
  testRunner.log('Got prerender frame: ' + !!prerenderFrameCommitted);
  testRunner.completeTest();
});
