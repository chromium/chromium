(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const pageURL = 'http://127.0.0.1:8000/inspector-protocol/resources/inspector-protocol-page.html';
  const {session, dp} = await testRunner.startURL(pageURL,
    'Tests enabling auto-attach on Browser with filter allowing both tab and page results in an error.');

  const bp = (await testRunner.attachFullBrowserSession()).protocol;

  bp.Target.onAttachedToTarget(event => {
    testRunner.log(event.params, 'FAIL: unexpected attachedToTarget event');
  });
  const err = await bp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true, flatten: true, filter: [{}]});
  testRunner.log(err, 'Expected error when auto-attaching with wildcard filter');
  testRunner.completeTest();
});
