(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Verifies that we can retrieve a request body consisting of blob in service worker.');
  const swHelper = (await testRunner.loadScript('resources/service-worker-helper.js'))(dp, session);

  let swdp = null;
  await dp.Target.setAutoAttach(
    {autoAttach: true, waitForDebuggerOnStart: false, flatten: true});
  dp.Target.onAttachedToTarget(async event => {
    swdp = session.createChild(event.params.sessionId).protocol;
  });

  const serviceWorkerURL = '/inspector-protocol/service-worker/resources/blank-service-worker.js';
  await session.navigate('resources/repeat-fetch-service-worker.html');
  await swHelper.installSWAndWaitForActivated(serviceWorkerURL);

  await dp.Page.enable();
  await dp.Page.reload();
  await swHelper.installSWAndWaitForActivated(serviceWorkerURL);
  await swdp.Network.enable();
  await swdp.Runtime.enable();
  const postBlobFromServiceWorker = `
      fetch('/', { method: "POST", body: new Blob(['Psychrolutes ', 'microporos']) });
  `;
  swdp.Runtime.evaluate({expression: postBlobFromServiceWorker});
  const requestId = (await swdp.Network.onceRequestWillBeSent()).params.requestId;
  const body = (await swdp.Network.getRequestPostData({requestId})).result.postData;
  testRunner.log(`post data: ${body}`);

  testRunner.completeTest();
});
