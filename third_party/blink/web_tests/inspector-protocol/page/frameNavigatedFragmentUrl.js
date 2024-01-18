(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that the url fragment is reported by Page.frameNavigated.');

  dp.Page.enable();

  function normalizeUrl(url) {
    if (typeof url === 'undefined') return undefined;
    return url.split('/').pop();
  }

  {
    testRunner.log('No Fragment');
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/simple-iframe.html')}';
      document.body.appendChild(frame);
    `);
    let result = await dp.Page.onceFrameNavigated();
    testRunner.log('url = ' + normalizeUrl(result.params.frame.url));
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
    testRunner.log('');
  }

  {
    testRunner.log('Empty Fragment');
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/simple-iframe.html#')}';
      document.body.appendChild(frame);
    `);
    let result = await dp.Page.onceFrameNavigated();
    testRunner.log('url = ' + normalizeUrl(result.params.frame.url));
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
    testRunner.log('');
  }

  {
    testRunner.log('Normal Fragment');
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/simple-iframe.html#fragment')}';
      document.body.appendChild(frame);
    `);
    let result = await dp.Page.onceFrameNavigated();

    testRunner.log('url = ' + normalizeUrl(result.params.frame.url));
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
    testRunner.log('');
  }

  {
    testRunner.log('Unreachable Fragment');
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/idont_exist.html#fragment')}';
      document.body.appendChild(frame);
    `);
    let result = await dp.Page.onceFrameNavigated();
    testRunner.log('url = ' + result.params.frame.url);
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
  }
  testRunner.completeTest();
})
