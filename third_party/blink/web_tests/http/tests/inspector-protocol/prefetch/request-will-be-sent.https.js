(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
        `Tests that Network.requestWillBeSent is dispatched for speculation-rule base prefetch requests.`);

  await dp.Network.enable();

  let testPromise = new Promise((resolve) => {
    let finalizedRequests = 0;
    let events = [];
    let requestIds = [];
    function store(event, title, order){
      const requestId = event.params.requestId;
      if(!requestIds.includes(requestId)) {
        requestIds.push(requestId)
      }
      events.push({id: requestId, title: title, order: order, params: event.params});
    }
    function checkResolve() {
      finalizedRequests++;
      if(finalizedRequests >= 2) {
        resolve({requestIds, events: events.sort((x,y)=>x.order-y.order)});
      }
    }
    dp.Network.onRequestWillBeSent(event => store(event, 'Network.onRequestWillBeSent', 0));
    dp.Network.onRequestWillBeSentExtraInfo(event => store(event, 'Network.onRequestWillBeSentExtraInfo', 1));
    dp.Network.onResponseReceived(event => store(event, 'Network.onResponseReceived', 2));
    dp.Network.onResponseReceivedExtraInfo(event => store(event, 'Network.onResponseReceivedExtraInfo', 3));
    dp.Network.onLoadingFinished(event => { store(event, 'Network.onLoadingFinished', 4); checkResolve();});
    dp.Network.onLoadingFailed(event => { store(event, 'Network.onLoadingFailed', 5); checkResolve();});
  });

  page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.https.html")

  let prefetchRequestId = await testPromise.then((result) => {
    let prefetchRequestId = undefined;
    const stabilizeNames = [...TestRunner.stabilizeNames, 'wallTime', 'requestTime', 'responseTime', 'Date', 'receiveHeadersStart', 'receiveHeadersEnd', 'sendStart', 'sendEnd', 'ETag', 'Last-Modified', 'User-Agent', 'headersText'];
    let {requestIds, events} = result;
    for(let i=0; i<requestIds.length; ++i) {
      testRunner.log(`Message ${i}`);
      events.forEach((event) => {
        if(event.id === requestIds[i]) {
          if(event.params?.type == 'Prefetch') {
            prefetchRequestId = requestIds[i];
          }
          testRunner.log(event.params, event.title, stabilizeNames);
        }
      });
    }
    return prefetchRequestId;
  });

  const msg = await dp.Network.getResponseBody({requestId: prefetchRequestId});
  const prefetchResponseBodyTitle = 'Prefetch response body';
  if(msg.error) {
    testRunner.log(msg.error, prefetchResponseBodyTitle);
  } else {
    testRunner.log(msg.result, prefetchResponseBodyTitle);
  }

  testRunner.completeTest();
})
