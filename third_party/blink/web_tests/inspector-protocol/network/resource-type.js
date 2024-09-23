(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that responseReceived contains the documented resource types.`);

  var resources = [
    { url: '/resources-page.html', responseAvailable: false },
    { url: '/stylesheet.css'},
    { url: '/script.js'},
    { url: '/abe.png'},
    { url: '/test.wav'},
    { url: '/test.ogv'},
    { url: '/simple-captions.vtt'},
    { url: '/greenbox.png'}
  ];
  var resourcesLeft = resources.length;

  testRunner.log('Test started');
  var messageObject = await dp.Network.enable();
  if (messageObject.error) {
    testRunner.log(`FAIL: Couldn't enable network agent: ${messageObject.error.message}`);
    testRunner.completeTest();
    return;
  }
  testRunner.log('Network agent enabled');

  dp.Network.onRequestWillBeSent(event => {
    var url = event.params.request.url;
    if (url.startsWith('blob'))
      return;
    if (url.startsWith('data'))
      return;

    var type = event.params.type;
    for (var resource of resources) {
      if (!url.endsWith(resource.url))
        continue;
      if (resource.gotRequestType)
        testRunner.fail('FAIL: Requested resource ' + url + ' twice.');
      resource.gotRequestType = type;
      return;
    }
    testRunner.fail('FAIL: Requested unexpected resource ' + url);
  });

  dp.Network.onResponseReceived(event => {
    var url = event.params.response.url;
    if (url.indexOf('blob') === 0)
      return;
    if (url.indexOf('data') === 0)
      return;

    var type = event.params.type;
    var requestId = event.params.requestId;
    for (var resource of resources) {
      if (url.substring(url.length - resource.url.length) !== resource.url)
        continue;
      if (resource.gotType)
        testRunner.fail('FAIL: Received resource ' + url + ' twice.');
      resource.gotType = type;
      resource.requestId = requestId;
      return;
    }
    testRunner.fail('FAIL: received unexpected resource ' + url);
  });

  dp.Network.onLoadingFinished(async event => {
    var requestId = event.params.requestId;
    for (var resource of resources) {
      if (resource.requestId !== requestId)
        continue;
      if (!('responseAvailable' in resource)) {
        var protocolResponse = await dp.Network.getResponseBody({requestId});
        if (protocolResponse.error) {
          resource.responseAvailable = false;
        } else {
          resource.responseAvailable = true;
          resource.responseEncoded = protocolResponse.result.base64Encoded;
        }
      }
      resourcesLeft -= 1;
      if (resourcesLeft === 0)
        receivedAllResources();
    }
  });

  function receivedAllResources() {
    for (var resource of resources) {
      var description = [
        'Resource ' + resource.url + ':',
        '  - request type: ' + resource.gotRequestType,
        '  - response type: ' + resource.gotType,
        '  - responseAvailable: ' + resource.responseAvailable
      ];
      if (resource.responseAvailable)
        description.push('  - responseEncoded: ' + resource.responseEncoded);

      testRunner.log(description.join('\n'));
    }
    testRunner.completeTest();
  }

  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('../resources/resources-page.html')}';
    document.body.appendChild(iframe);
  `);
})
