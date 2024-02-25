(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that the unreachable url is reported when navigating to a ' +
      'nonexistent page.');

  dp.Page.enable();
  session.evaluate(`
    var frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/idont_exist.html')}';
    document.body.appendChild(frame);
  `);
  var result = await dp.Page.onceFrameNavigated();
  testRunner.log('Page navigated, url = ' + result.params.frame.url);
  testRunner.log('UnreachableUrl = ' +
      result.params.frame.unreachableUrl.split('/').pop());
  testRunner.completeTest();
})
