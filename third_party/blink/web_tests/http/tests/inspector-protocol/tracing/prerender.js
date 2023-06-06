(async function(testRunner) {
  const pageUrl =
      'http://127.0.0.1:8000/inspector-protocol/prerender/resources/inspector-protocol-page.html';
  const {page, session, dp} = await testRunner.startURL(
      pageUrl, 'Test that prerender page is included in the trace events');

  const bp = testRunner.browserP();
  const tabs = (await bp.Target.getTargets({
                 filter: [{type: 'tab'}]
               })).result.targetInfos;
  const tabUnderTest = tabs.find(target => target.url === pageUrl);

  const tabSessionId = (await bp.Target.attachToTarget({
                         targetId: tabUnderTest.targetId,
                         flatten: true
                       })).result.sessionId;
  const tabSession = testRunner.browserSession().createChild(tabSessionId);
  const tp = tabSession.protocol;

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const tracingHelper = new TracingHelper(testRunner, tabSession);

  await dp.Preload.enable();
  await tracingHelper.startTracing();

  const prerenderReadyPromise = dp.Preload.oncePrerenderStatusUpdated(e => e.params.status == 'Ready');
  tp.Target.setAutoAttach({autoAttach: true, flatten: true, waitForDebuggerOnStart: false});
  page.navigate('../prerender/resources/simple-prerender.html');
  const prerenderSessionId = (await tp.Target.onceAttachedToTarget(
      e => e.params.targetInfo.subtype === 'prerender')).params.sessionId;
  const pp = session.createChild(prerenderSessionId).protocol;
  await Promise.all([pp.Preload.enable(), prerenderReadyPromise]);
  session.evaluate(`document.getElementById('link').click()`);

  await pp.Preload.oncePrerenderAttemptCompleted();

  const devtoolsEvents = await tracingHelper.stopTracing();
  const prerenderFrameCommitted =
      tracingHelper
          .findEvents('FrameCommittedInBrowser', TracingHelper.Phase.INSTANT)
          .find(
              e => e.args.data.url ===
                  'http://127.0.0.1:8000/inspector-protocol/prerender/resources/empty.html');
  testRunner.log('Got prerender frame: ' + !!prerenderFrameCommitted);
  const prerenderFrameId = prerenderFrameCommitted.args.data.frame;

  const parsePrerenderHTML =
      tracingHelper.findEvents('ParseHTML', TracingHelper.Phase.COMPLETE)
          .find(e => e.args.beginData.frame === prerenderFrameId);

  testRunner.log('Got parse prerender HTML: ' + !!parsePrerenderHTML);
  testRunner.completeTest();
});
