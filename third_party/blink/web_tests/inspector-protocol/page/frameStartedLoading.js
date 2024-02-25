(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests frameStartedLoading/frameStoppedLoading events.');

  dp.Page.enable();
  session.evaluate(`
    var frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);
  await dp.Page.onceFrameStartedLoading();
  testRunner.log('Started loading');
  await dp.Page.onceFrameStoppedLoading();
  testRunner.log('Stopped loading');
  testRunner.completeTest();
})
