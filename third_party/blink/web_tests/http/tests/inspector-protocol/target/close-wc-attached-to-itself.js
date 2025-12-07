(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      'Tests that closing a web contents that is attached to itself doesn\'t crash.');

  const {sessionId} =
      (await testRunner.browserP().Target.attachToBrowserTarget()).result;
  const browserSession = testRunner.browserSession().createChild(sessionId);
  const bp = browserSession.protocol;

  const targetId =
      (await bp.Target.createTarget({url: 'about:blank', hidden: true}))
          .result.targetId;
  const response = await bp.Target.attachToTarget({targetId, flatten: true});
  const childSession = browserSession.createChild(response.result.sessionId);
  const dp2 = childSession.protocol;
  dp2.Runtime.enable();
  dp2.Runtime.onConsoleAPICalled(e => testRunner.log(e.params.args[0].value));
  await bp.Target.exposeDevToolsProtocol(
      {targetId: targetId, bindingName: 'cdp'});
  const testFrameworkURL =
      new URL(
          '/inspector-protocol/resources/inspector-protocol-test-subtarget.html?mark',
          location.href)
          .href;
  await childSession.navigate(testFrameworkURL);

  async function attachToSelf() {
    const testBaseURL = '';
    const targetBaseURL = '';
    const params = {};
    const testRunner = new TestRunner(
        testBaseURL, targetBaseURL, DevToolsAPI._log, DevToolsAPI._completeTest,
        DevToolsAPI._fetch, params);
    const autoAttachParams = {
      autoAttach: true,
      waitForDebuggerOnStart: false,
      flatten: true,
    };
    const bp = testRunner.browserP();
    bp.Target.setAutoAttach({...autoAttachParams, filter: [{type: 'tab'}]});
    const event = await bp.Target.onceAttachedToTarget(
        event => event.params.targetInfo.url.endsWith(
            'inspector-protocol-test-subtarget.html?mark'));
    // We need two sessions to be attached, as the condition requires
    // auto_attach() be on (i.e. one client still be attached while another one
    // is being detached).
    const session1 =
        testRunner.browserSession().createChild(event.params.sessionId);
    const tp1 = session1.protocol.Target.setAutoAttach(autoAttachParams);
    const attached2 = (await bp.Target.attachToTarget(
        {targetId: event.params.targetInfo.targetId, flatten: true}));
    const session2 =
        testRunner.browserSession().createChild(attached2.result.sessionId);
    await session2.protocol.Target.setAutoAttach(autoAttachParams);
    testRunner.log('attached');
    return output;
  }

  testRunner.log(await childSession.evaluateAsync(attachToSelf));

  // The attach should be announced to the client that issued setAutoAttach,
  // which is the page itself. But since the page is also the thing being
  // attached to, it also receives the notification.
  // The important thing is that the browser doesn't crash.
  browserSession.disconnect();

  testRunner.log('Target closed.');
  testRunner.completeTest();
})
