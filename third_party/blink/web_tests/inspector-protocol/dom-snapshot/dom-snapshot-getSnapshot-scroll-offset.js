(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-scroll-offset.html', 'Tests DOMSnapshot.getSnapshot reports scroll offset and bounding box in terms of document coordinates.');

  const response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': []});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(response.result, null, ['documentURL', 'baseURL', 'frameId', 'backendNodeId']);
  testRunner.completeTest();
})
