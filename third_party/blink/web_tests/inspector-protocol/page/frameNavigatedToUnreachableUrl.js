(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that the unreachable url is reported when navigating to a ' +
      'nonexistent page.');

  await dp.Page.enable();
  await dp.Target.setAutoAttach(
      {autoAttach: true, waitForDebuggerOnStart: true, flatten: true});

  let navigatedPromise = new Promise(resolve => {
    dp.Page.onFrameNavigated(event => resolve(event));
    dp.Target.onAttachedToTarget(async event => {
      const dp2 = session.createChild(event.params.sessionId).protocol;
      await dp2.Page.enable();
      dp2.Page.onFrameNavigated(e => resolve(e));
      await dp2.Runtime.runIfWaitingForDebugger();
    });
  });

  session.evaluate(`
    var frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/idont_exist.html')}';
    document.body.appendChild(frame);
  `);
  var result = await navigatedPromise;
  testRunner.log('Page navigated, url = ' + result.params.frame.url);
  testRunner.log('UnreachableUrl = ' +
      result.params.frame.unreachableUrl.split('/').pop());
  testRunner.completeTest();
})
