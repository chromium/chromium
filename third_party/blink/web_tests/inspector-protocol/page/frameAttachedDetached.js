(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests frame lifetime events.');

  dp.Page.enable();
  dp.Network.enable();
  dp.Network.onRequestWillBeSent(e => {
    testRunner.log(`RequestWillBeSent ${testRunner.trimURL(e.params.request.url)}`);
  });
  testRunner.log('Creating iframe "blank.html"');
  session.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);
  testRunner.log(await dp.Page.onceFrameAttached(), '', ['params', 'sessionId']);
  testRunner.log(await dp.Page.onceFrameStartedLoading());
  testRunner.log(await dp.Page.onceFrameNavigated(), '', ['params', 'sessionId']);
  testRunner.log(await dp.Page.onceFrameStoppedLoading());
  testRunner.log('Navigating iframe to "about:blank"');
  session.evaluate('frame.src = "about:blank"');
  testRunner.log(await dp.Page.onceFrameStartedLoading());
  testRunner.log(await dp.Page.onceFrameNavigated(), '', ['params', 'sessionId']);
  testRunner.log('Removing iframe');
  session.evaluate('document.body.removeChild(frame);');
  testRunner.log(await dp.Page.onceFrameDetached());
  testRunner.completeTest();
})
