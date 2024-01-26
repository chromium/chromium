(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pageURL = 'http://127.0.0.1:8000/inspector-protocol/resources/inspector-protocol-page.html';

  await testRunner.startURL(pageURL, 'Tests auto-attahcing tab targets from browser targets.');

  const browserSession = await testRunner.attachFullBrowserSession();
  const bp = browserSession.protocol;

  await bp.Fetch.enable({});
  bp.Fetch.onRequestPaused(e => {
    testRunner.log(`Requesting ${e.params.request.url}`);
    bp.Fetch.continueRequest({requestId: e.params.requestId});
  });
  bp.Target.onAttachedToTarget(({params}) => {
    // Pretend the test page is not there.
    if (params.targetInfo.url === location.href)
      return;
    testRunner.log(params, 'Attached to target: ');
  });
  const enableAutoAttachPromise = bp.Target.setAutoAttach({
      autoAttach: true,
      waitForDebuggerOnStart: true,
      flatten: true,
      filter: [{'type': 'tab'}]
  });

  {
    // Assure issuing runIfWaitingForDebugger to a non-paused target is not
    // an error, for compatibility with other targets.
    const {params} = await bp.Target.onceAttachedToTarget();
    const tp = browserSession.createChild(params.sessionId).protocol;
    const response = await tp.Runtime.runIfWaitingForDebugger();
    testRunner.log(response, 'Response to runIfWaitingForDebugger: ');
  }

  await enableAutoAttachPromise;
  const newUrl = testRunner.url('../resources/test-page.html');
  testRunner.log('Creating a new target, expect it paused');
  const createTargetPromise = bp.Target.createTarget({url: newUrl});
  const tabTarget = (await bp.Target.onceAttachedToTarget()).params;
  await createTargetPromise;

  const tabSession = browserSession.createChild(tabTarget.sessionId);
  const tp = tabSession.protocol;
  tp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});
  const pageTarget = (await tp.Target.onceAttachedToTarget()).params;
  testRunner.log(pageTarget, 'Page attached via tab target: ');
  const pageSession = browserSession.createChild(pageTarget.sessionId);
  pageSession.protocol.Page.enable();

  testRunner.log('Resuming target');
  const response = tp.Runtime.runIfWaitingForDebugger();
  await pageSession.protocol.Page.onceLoadEventFired();
  testRunner.log(await response, 'Response to runIfWaitingForDebugger: ');
  testRunner.log('load event fired');
  testRunner.completeTest();
});
