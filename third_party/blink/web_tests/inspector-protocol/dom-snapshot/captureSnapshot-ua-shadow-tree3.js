(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <html>
  <style>
   body { font: 10px Ahem; }
  </style>
  <body>
  <textarea rows="10" cols="30">We apologize for the inconvenience.</textarea>
  </body>
  </html>`, 'Tests that DOMSnapshot properly descends into the shadow tree');

  var response = await dp.DOMSnapshot.captureSnapshot({computedStyles: []});
  if (response.error) {
    testRunner.log(response);
  } else {
    response.result.strings[response.result.documents[0].documentURL] = '';
    response.result.strings[response.result.documents[0].frameId] = '';
    testRunner.log(
        response.result, undefined,
        ['documentURL', 'frameId', 'backendNodeId', 'bounds', 'shadowRootType']);
  }
  testRunner.completeTest();
})
