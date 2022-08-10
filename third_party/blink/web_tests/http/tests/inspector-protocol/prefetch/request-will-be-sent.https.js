(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
        `Tests that Network.requestWillBeSent is dispatched for speculation-rule base prefetch requests.`);

  await dp.Network.enable();

  let testPromise = new Promise((resolve) => {
    let finalizedRequests = 0;
    let events = [];
    let requestIds = [];
    function store(event, title){
      const requestId = event.params.requestId;
      if(!requestIds.includes(requestId)) {
        requestIds.push(requestId)
      }
      events.push({id: requestId, title: title, params: event.params});
    }
    function checkResolve() {
      finalizedRequests++;
      if(finalizedRequests >= 2) {
        resolve({requestIds, events});
      }
    }
    dp.Network.onRequestWillBeSent(event => store(event, 'Network.onRequestWillBeSent'));
    dp.Network.onRequestWillBeSentExtraInfo(event => store(event, 'Network.onRequestWillBeSentExtraInfo'));
    dp.Network.onResponseReceived(event => store(event, 'Network.onResponseReceived'));
    dp.Network.onLoadingFinished(event => { store(event, 'Network.onLoadingFinished'); checkResolve();});
    dp.Network.onLoadingFailed(event => { store(event, 'Network.onLoadingFailed'); checkResolve();});
  });

  page.navigate("https://127.0.0.1:8443/inspector-protocol/prefetch/resources/prefetch.https.html")

  let prefetchRequestId = await testPromise.then((result) => {
    let prefetchRequestId = undefined;
    const stabilizeNames = [...TestRunner.stabilizeNames, 'wallTime', 'requestTime', 'responseTime', 'Date', 'receiveHeadersEnd', 'sendStart', 'sendEnd', 'ETag', 'Last-Modified', 'User-Agent'];
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
