(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <html>
  <style>
   body { font: 10px Ahem; }
  </style>
  <body>
  <img id="one" src="http://example.com/A.png"/>
  <img id="two" src="http://example.com/B.png"/>
  <img id="three" src="http://example.com/C.png"/>
  </body>
  </html>`, 'Tests that DOMSnapshot skips UA shadow root when traversing the DOM tree');

  var response = await dp.DOMSnapshot.captureSnapshot({computedStyles: []});
  if (response.error) {
    testRunner.log(response);
  } else {
    response.result.strings[response.result.documents[0].documentURL] = '';
    response.result.strings[response.result.documents[0].frameId] = '';
    testRunner.log(
        response.result, undefined,
        ['documentURL', 'frameId', 'backendNodeId', 'shadowRootType']);
  }
  testRunner.completeTest();
})
