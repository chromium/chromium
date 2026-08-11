(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that the url fragment is reported by Page.frameNavigated.');

  await dp.Page.enable();
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let navigationCallback = null;
  function onNavigated(event) {
    if (navigationCallback) {
      const callback = navigationCallback;
      navigationCallback = null;
      callback(event);
    }
  }

  dp.Page.onFrameNavigated(onNavigated);
  dp.Target.onAttachedToTarget(async event => {
    const dp2 = session.createChild(event.params.sessionId).protocol;
    await dp2.Page.enable();
    dp2.Page.onFrameNavigated(onNavigated);
    await dp2.Runtime.runIfWaitingForDebugger();
  });

  function waitForNavigation() {
    return new Promise(resolve => {
      navigationCallback = resolve;
    });
  }

  function normalizeUrl(url) {
    if (typeof url === 'undefined') return undefined;
    return url.split('/').pop();
  }

  {
    testRunner.log('No Fragment');
    const navPromise = waitForNavigation();
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/simple-iframe.html')}';
      document.body.appendChild(frame);
    `);
    let result = await navPromise;
    testRunner.log('url = ' + normalizeUrl(result.params.frame.url));
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
    testRunner.log('');
  }

  {
    testRunner.log('Empty Fragment');
    const navPromise = waitForNavigation();
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/simple-iframe.html#')}';
      document.body.appendChild(frame);
    `);
    let result = await navPromise;
    testRunner.log('url = ' + normalizeUrl(result.params.frame.url));
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
    testRunner.log('');
  }

  {
    testRunner.log('Normal Fragment');
    const navPromise = waitForNavigation();
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/simple-iframe.html#fragment')}';
      document.body.appendChild(frame);
    `);
    let result = await navPromise;

    testRunner.log('url = ' + normalizeUrl(result.params.frame.url));
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
    testRunner.log('');
  }

  {
    testRunner.log('Unreachable Fragment');
    const navPromise = waitForNavigation();
    session.evaluate(`
      var frame = document.createElement('iframe');
      frame.src = '${testRunner.url('../resources/idont_exist.html#fragment')}';
      document.body.appendChild(frame);
    `);
    let result = await navPromise;
    testRunner.log('url = ' + result.params.frame.url);
    testRunner.log('urlFragment = ' + result.params.frame.urlFragment);
    testRunner.log('UnreachableUrl = ' + normalizeUrl(result.params.frame.unreachableUrl));
  }
  testRunner.completeTest();
})
