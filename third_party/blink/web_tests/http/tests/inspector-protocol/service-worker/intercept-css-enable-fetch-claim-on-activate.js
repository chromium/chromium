(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/repeat-fetch-service-worker.html',
      'Verifies that service workers do not throw errors from devtools css enable initiated fetch.');
  const swHelper = (await testRunner.loadScript('resources/service-worker-helper.js'))(dp, session);

  dp.ServiceWorker.onWorkerErrorReported(error => {
    testRunner.log(
        'service worker reported error: ' + JSON.stringify(error.params, null, 2));
    testRunner.completeTest();
  });

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();

  await swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/claim-on-activate-service-worker.js');
  await dp.Page.enable();
  await dp.Page.reload();
  await swHelper.installSWAndWaitForActivated('/inspector-protocol/service-worker/resources/claim-on-activate-service-worker.js');

  await dp.DOM.enable();
  await dp.CSS.enable();
  testRunner.log('finished awaiting for CSS.enable');

  // Also make sure changes from service workers are seen by CSS.enable
  const getResourceTreeResponse = await dp.Page.getResourceTree();
  const mainFrameId = getResourceTreeResponse.result.frameTree.frame.id;
  const getResourceContentResponse = await dp.Page.getResourceContent({
    frameId: mainFrameId,
    url:
        'http://127.0.0.1:8000/inspector-protocol/service-worker/resources/repeat-fetch-service-worker.html'
  });
  testRunner.log(
      'document resource from CSS.enable:\n' +
      getResourceContentResponse.result.content);
  testRunner.completeTest();
});
