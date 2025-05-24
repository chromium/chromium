(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
      <iframe id="frame"
          src="http://127.0.0.1:8000/inspector-protocol/resources/empty.html?1">
      </iframe>`,
  'Tests that a local subframe is not detached upon navigation');

  await dp.Page.enable();

  function logEvent(e) {
    const url = e.params.url || e.params.frame?.url || "";
    testRunner.log(`${e.method} ${url}`);
  }

  dp.Page.onFrameRequestedNavigation(logEvent);
  dp.Page.onFrameStartedNavigating(logEvent);
  dp.Page.onFrameDetached(e => {
    testRunner.log(`${FAIL} ${e.method}`);
  });
  dp.Page.onFrameNavigated(logEvent);

  session.evaluate(`
      document.getElementById('frame').src =
          'http://127.0.0.1:8000/inspector-protocol/resources/empty.html?2'
  `);

  await dp.Page.onceFrameNavigated();
  testRunner.completeTest();
})
