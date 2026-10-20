(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startURL(
      'resources/repeat-fetch-service-worker.html',
      'Tests that response body is available after a response provided by a service worker, or fetched by it, is consumed as a blob.');
  const swHelper =
      (await testRunner.loadScript('resources/service-worker-helper.js'))(
          dp, session);

  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  const attachedPromise = dp.Target.onceAttachedToTarget();
  await swHelper.installSWAndWaitForActivated(
      '/inspector-protocol/service-worker/resources/respond-with-service-worker.js');
  const swSession =
      session.createChild((await attachedPromise).params.sessionId);

  async function logResponseBody(target, session, url) {
    const dp = session.protocol;
    const blobSizePromise = session.evaluateAsync(
        `fetch('${url}').then(r => r.blob()).then(b => b.size)`);
    const requestId =
        (await dp.Network.onceRequestWillBeSent()).params.requestId;
    await dp.Network.onceLoadingFinished(e => e.params.requestId === requestId);
    const response = await dp.Network.getResponseBody({requestId});
    testRunner.log(
        `${target} fetched ${url}, blob size: ${await blobSizePromise}, body:`);
    testRunner.log(response.error ? response.error.message :
                                    response.result.body);
  }

  await dp.Network.enable();
  await logResponseBody('page', session, 'synthesized');
  await logResponseBody('page', session, 'hello-world.txt');

  await swSession.protocol.Network.enable();
  await logResponseBody('service worker', swSession, 'hello-world.txt');

  testRunner.completeTest();
})
