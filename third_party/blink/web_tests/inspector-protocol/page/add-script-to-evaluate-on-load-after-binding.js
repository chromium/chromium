(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp, session } = await testRunner.startBlank(
    `Tests that Page.addScriptToEvaluateOnLoad is executed on the initial about:blank document of a new target.`);

  const bp = testRunner.browserP();
  await bp.Target.setAutoAttach({ autoAttach: true, waitForDebuggerOnStart: true, flatten: true });

  bp.Target.createTarget({ url: 'about:blank' });

  const attachedEvent = await bp.Target.onceAttachedToTarget();
  const sessionId = attachedEvent.params.sessionId;

  const childSession = session.createChild(sessionId);
  const dp2 = childSession.protocol;

  dp2.Runtime.addBinding({ name: 'frobnicate' });
  dp2.Page.addScriptToEvaluateOnNewDocument({
    source: `frobnicate("foo"); window.temp = 123;`,
    runImmediately: true,
  });

  const resultPromise = await childSession.evaluate(`window.temp`);

  testRunner.log('Result: ' + resultPromise);

  testRunner.completeTest();
})
