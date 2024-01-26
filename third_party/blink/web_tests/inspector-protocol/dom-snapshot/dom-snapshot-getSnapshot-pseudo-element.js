(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-pseudo-element.html', 'Tests DOMSnapshot.getSnapshot exports layout tree nodes associated with pseudo elements.');

  const response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': ['font-weight', 'color'], 'includeEventListeners': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(response.result, null, ['documentURL', 'baseURL', 'frameId', 'backendNodeId']);
  testRunner.completeTest();
})
