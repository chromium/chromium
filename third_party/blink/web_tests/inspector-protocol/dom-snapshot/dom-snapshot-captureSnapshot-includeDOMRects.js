(async function(testRunner) {
  const {dp} = await testRunner.startURL('../resources/dom-snapshot-includeDOMRects.html', 'Tests DOMSnapshot.getSnapshot reports offset, scroll, and client rects of each node.');

  function stabilize(key, value) {
    const unstableKeys = new Set(['strings', 'textBoxes', 'nodes', 'nodeIndex', 'styles', 'text', 'stackingContexts']);
    if (unstableKeys.has(key))
      return '<' + typeof(value) + '>';
    return value;
  }

  const response = await dp.DOMSnapshot.captureSnapshot({'computedStyles': [], 'includeDOMRects': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(JSON.stringify(response.result, stabilize, 2));
  testRunner.completeTest();
})
