(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-pseudo-element.html', 'Tests DOMSnapshot.getSnapshot exports layout tree nodes associated with pseudo elements.');

  var response = await dp.DOMSnapshot.captureSnapshot({'computedStyles': ['font-weight', 'color'], 'includeEventListeners': true});
  if (response.error) {
    testRunner.log(response);
  } else {
    // Remove unstable strings from the string table.
    response.result.strings[response.result.documents[0].documentURL] = '';
    response.result.strings[response.result.documents[0].baseURL] = '';
    response.result.strings[response.result.documents[0].frameId] = '';
    testRunner.log(
        response.result, undefined,
        ['documentURL', 'baseURL', 'frameId', 'backendNodeId']);
  }
  testRunner.completeTest();
})
