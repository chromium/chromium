(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that multiple sessions receives the same frame lifetime notifications.');
  var page = await testRunner.createPage();
  var session1 = await page.createSession();
  var session2 = await page.createSession();

  session1.protocol.Page.enable();
  session2.protocol.Page.enable();

  testRunner.log('\nAttaching frame in 1');
  session1.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);
  var attachedBoth = Promise.all([session1.protocol.Page.onceFrameAttached(), session2.protocol.Page.onceFrameAttached()]);
  var startedLoadingBoth = Promise.all([session1.protocol.Page.onceFrameStartedLoading(), session2.protocol.Page.onceFrameStartedLoading()]);
  var stopLoadingBoth = Promise.all([session1.protocol.Page.onceFrameStoppedLoading(), session2.protocol.Page.onceFrameStoppedLoading()]);
  var navigatedBoth = Promise.all([session1.protocol.Page.onceFrameNavigated(), session2.protocol.Page.onceFrameNavigated()]);

  await attachedBoth;
  testRunner.log('Attached in both');

  await startedLoadingBoth;
  testRunner.log('Started loading in both');

  await navigatedBoth;
  testRunner.log('Navigated in both');

  await stopLoadingBoth;
  testRunner.log('\nNavigating frame in 2');
  session2.evaluate('frame.src = "about:blank"');
  startedLoadingBoth = Promise.all([session1.protocol.Page.onceFrameStartedLoading(), session2.protocol.Page.onceFrameStartedLoading()]);
  navigatedBoth = Promise.all([session1.protocol.Page.onceFrameNavigated(), session2.protocol.Page.onceFrameNavigated()]);

  await startedLoadingBoth;
  testRunner.log('Started loading in both');

  await navigatedBoth;
  testRunner.log('Navigated in both');

  testRunner.log('\nDetaching frame in 2');
  session2.evaluate('document.body.removeChild(frame);');

  var detachedBoth = Promise.all([session1.protocol.Page.onceFrameDetached(), session2.protocol.Page.onceFrameDetached()]);
  await detachedBoth;
  testRunner.log('Detached in both');

  testRunner.completeTest();
})
