(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/websocket-initiator.html', `Initiator for Websockets check fixes http://crbug.com/457811`);

  function cleanUrl(url) {
    url = url.match(/\/[^\/]+$/);
    if (url.length)
      return url[0].substr(1);
    return url;
  }

  testRunner.log('Test started');
  await dp.Network.enable();
  testRunner.log('Network agent enabled');

  session.evaluate(`createSocket()`);

  var event = await dp.Network.onceWebSocketCreated();
  var initiator = event.params.initiator;
  testRunner.log('');
  testRunner.log('Initiator Type: ' + initiator.type);
  var callFrames = initiator.stack ? initiator.stack.callFrames : [];
  for (var i = 0; i < callFrames.length; ++i) {
    var frame = callFrames[i];
    testRunner.log('Stack #' + i);
    if (frame.lineNumber) {
      testRunner.log('  functionName: ' + frame.functionName);
      testRunner.log('  url: ' + cleanUrl(frame.url));
      testRunner.log('  lineNumber: ' + frame.lineNumber);
      break;
    }
  }
  testRunner.completeTest();
})
