(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
        'Tests that appropriate Network domain events are dispatched during speculation rules prefetch while following redirects');

  const gatherNetworkEvents = await testRunner.loadScript('resources/gather-prefetch-network.js');
  const events = gatherNetworkEvents(testRunner, dp, { requests: 2, log: true });

  const redirectDestination = "/resources/blank.html";
  const redirect = `/resources/redirect.php?url=${encodeURIComponent(redirectDestination)}`;
  page.navigate(`https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.https.html?url=${encodeURIComponent(redirect)}`);

  const prefetchRequestId = (await events).find(event => event.params.type === 'Prefetch')?.params.requestId;
  const msg = await dp.Network.getResponseBody({requestId: prefetchRequestId});
  if (msg.error) {
    testRunner.log(msg.error, 'Unable to get prefetch response body');
  } else {
    testRunner.log(msg.result, 'Prefetch response body');
  }

   testRunner.completeTest();
})
