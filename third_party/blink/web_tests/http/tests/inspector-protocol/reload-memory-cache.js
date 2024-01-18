(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/reload-memory-cache.html',
      `Tests that reloads when coming from protocol still use memory cache.`);

  await dp.Network.enable();
  dp.Page.reload({ignoreCache: false});

  var blankRequestId = 0;

  dp.Network.onRequestWillBeSent(request => {
    var url = request.params.request.url;
    if (/blank\.js$/.test(url)) {
      testRunner.log('Request Will be Sent for ' + url.substr(url.lastIndexOf('blank.js')));
      blankRequestId = request.params.requestId;
    }
  });

  dp.Network.onResponseReceived(request => {
    var url = request.params.response.url;
    if (/blank.js$/.test(url)) {
      testRunner.log('This should never be reached.');
      testRunner.completeTest();
    }
  });

  dp.Network.onRequestServedFromCache(request => {
    if (request.params.requestId === blankRequestId) {
      testRunner.log('Served From Cache for blank.js');
      testRunner.completeTest();
    }
  });
})
