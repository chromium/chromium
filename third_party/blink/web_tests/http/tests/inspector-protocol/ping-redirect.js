(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that redirect from navigator.sendBeacon() is recorded.`);

  function parseURL(url) {
    var result = {};
    var match = url.match(/^([^:]+):\/\/([^\/:]*)(?::([\d]+))?(?:(\/[^#]*)(?:#(.*))?)?$/i);
    if (!match)
      return result;
    result.scheme = match[1].toLowerCase();
    result.host = match[2];
    result.port = match[3];
    result.path = match[4] || "/";
    result.fragment = match[5];
    return result;
  }

  await dp.Network.enable();
  session.evaluate(`
    navigator.sendBeacon('${testRunner.url('resources/ping-redirect.php')}', 'foo');
  `);

  var requestSent = 0;
  dp.Network.onRequestWillBeSent(event => {
    requestSent++;
    var params = event.params;
    testRunner.log('Request Sent: ' + parseURL(params.request.url).path);
    if (requestSent == 2) {
      var redirectSource = '';
      if (params.redirectResponse)
        redirectSource = parseURL(params.redirectResponse.url).path;
      testRunner.log('Redirect Source: ' + redirectSource);
      testRunner.completeTest();
    }
  });
})
