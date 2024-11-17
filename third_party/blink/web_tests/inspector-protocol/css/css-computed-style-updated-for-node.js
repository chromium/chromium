(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { page, session, dp } = await testRunner.startURL(
    'resources/css-computed-style-updated-for-node.html',
    'Test CSS.trackComputedStyleUpdatesForNode method and CSS.computedStyleUpdated event');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const firstNodeId = await cssHelper.requestNodeId(documentNodeId, '.item1');

  await dp.CSS.trackComputedStyleUpdatesForNode({ nodeId: firstNodeId });
  const eventPromise = dp.CSS.onceComputedStyleUpdated();
  await session.evaluate(() => document.querySelector('.item1').style.backgroundColor = 'purple');
  testRunner.log('Updated element on the page');
  await eventPromise;
  testRunner.log(`Received event for the tracked node`);

  testRunner.completeTest();
});
