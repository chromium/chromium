(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('../resources/dom-snapshot-includeDOMRects.html', 'Tests DOMSnapshot.getSnapshot reports offset, scroll, and client rects of each node.');

  const response = await dp.DOMSnapshot.captureSnapshot({'computedStyles': [], 'includeDOMRects': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(response.result, null, ['strings', 'textBoxes', 'nodes', 'nodeIndex', 'styles', 'text', 'stackingContexts']);
  testRunner.completeTest();
})
