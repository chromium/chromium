(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-captureSnapshot-details.html', 'Tests that DOMSnapshot.captureSnapshot fills document details: title, size, etc.');

  var response = await dp.DOMSnapshot.captureSnapshot({computedStyles: []});
  if (response.error) {
    testRunner.log(response);
  } else {
    // Remove unstable strings from the string table.
    response.result.strings[response.result.documents[0].documentURL] = '';
    response.result.strings[response.result.documents[0].frameId] = '';
    testRunner.log(
        response.result, undefined,
        ['documentURL', 'frameId', 'backendNodeId']);
  }
  testRunner.completeTest();
})
