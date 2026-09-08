(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/blank.html',
      'Tests that opening DevTools (CSS.enable) on a file:// URL does not produce a security warning.');

  await dp.Log.enable();
  dp.Log.onEntryAdded(event => {
    testRunner.log('Unexpected Log entry: ' + event.params.entry.text);
  });

  await dp.Page.enable();
  await dp.DOM.enable();
  await dp.CSS.enable();
  testRunner.log('CSS.enable completed');

  const getResourceTreeResponse = await dp.Page.getResourceTree();
  const mainFrame = getResourceTreeResponse.result.frameTree.frame;
  const getResourceContentResponse = await dp.Page.getResourceContent({
    frameId: mainFrame.id,
    url: mainFrame.url,
  });
  testRunner.log('Page.getResourceContent succeeded: ' + (getResourceContentResponse.result.content.length > 0));
  testRunner.completeTest();
});
