(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that waitForDebuggerOnStart works with out-of-process iframes.`);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  await Promise.all([
    dp.Page.enable(),
    dp.Network.enable()
  ]);
  await dp.Network.setUserAgentOverride({userAgent: 'test'});
  const requestSentPromise = dp.Network.onceRequestWillBeSent();
  const attachedPromise = dp.Target.onceAttachedToTarget();
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = 'http://devtools.oopif.test:8000/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_USER_AGENT';
    document.body.appendChild(iframe);
  `);

  const params = (await requestSentPromise).params;
  const sessionId = (await attachedPromise).params.sessionId;
  const dp1 = session.createChild(sessionId).protocol;
  await dp1.Network.enable();
  await dp1.Network.setUserAgentOverride({userAgent: 'test (subframe)'});

  const loadingFinishedPromise = dp1.Network.onceLoadingFinished();
  await dp1.Runtime.runIfWaitingForDebugger();
  testRunner.log(`User-Agent = ${params.request.headers['User-Agent']}`);
  await loadingFinishedPromise;
  const content = (await dp1.Network.getResponseBody({requestId: params.requestId})).result.body;
  testRunner.log(`content (should have user-agent header overriden): ${content}`);

  testRunner.completeTest();
})
