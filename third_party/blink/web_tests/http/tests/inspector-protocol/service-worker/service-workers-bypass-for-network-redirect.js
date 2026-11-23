(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests "Bypass for network" (Network.setBypassServiceWorker) with redirection doesn't cause crash.`);

  await dp.Storage.clearDataForOrigin(
      {origin: 'http://127.0.0.1:8000', storageTypes: 'all'});
  await dp.Storage.clearDataForOrigin(
      {origin: 'http://localhost:8000', storageTypes: 'all'});

  await dp.Network.enable();
  await dp.Runtime.enable();

  const bypassResult = await dp.Network.setBypassServiceWorker({bypass: true});
  if (bypassResult.error) {
    testRunner.fail(
        `setBypassServiceWorker failed: ${JSON.stringify(bypassResult.error)}`);
    return;
  }

  const startUrl =
      'http://localhost:8000/inspector-protocol/service-worker/resources/' +
      'bypass-for-network-redirect.php';

  const events = [];
  let targetRequestId = null;
  let resolveLoadingFinished;
  const loadingFinished = new Promise(resolve => {
    resolveLoadingFinished = resolve;
  });

  dp.Network.onRequestWillBeSent(event => {
    const {requestId, request, redirectResponse} = event.params;
    if (request.url === startUrl) {
      targetRequestId = requestId;
      events.push(`request: ${request.url}`);
    } else if (redirectResponse && requestId === targetRequestId) {
      events.push(`redirect (${redirectResponse.status}): ${
          redirectResponse.url} -> ${request.url}`);
    } else {
      testRunner.fail(`Unexpected request: ${request.url}`);
    }
  });

  dp.Network.onResponseReceived(event => {
    if (event.params.requestId === targetRequestId) {
      events.push(`response (${event.params.response.status}): ${
          event.params.response.url}`);
    }
  });

  dp.Network.onLoadingFinished(event => {
    if (event.params.requestId === targetRequestId) {
      events.push('loadingFinished');
      resolveLoadingFinished();
    }
  });

  dp.Network.onLoadingFailed(event => {
    testRunner.fail(`Request failed (${event.params.requestId}): ${
        event.params.errorText}`);
  });

  const consolePromise = dp.Runtime.onceConsoleAPICalled(
      event => event.params.type === 'log' &&
          event.params.args[0]?.value === 'getRegistration finished');

  await session.evaluate(url => {
    const iframe = document.createElement('iframe');
    iframe.id = 'frame_id';
    iframe.src = url;
    document.body.appendChild(iframe);
  }, startUrl);

  const consoleEvent = await consolePromise;
  await loadingFinished;

  for (const line of events) {
    testRunner.log(line);
  }
  testRunner.log(`console.log: ${consoleEvent.params.args[0].value}`);
  testRunner.log('Success');
  testRunner.completeTest();
})
