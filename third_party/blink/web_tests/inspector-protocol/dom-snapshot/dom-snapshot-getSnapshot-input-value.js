(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-input-value.html', 'Tests DOMSnapshot.getSnapshot method returning input values.');

  const response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': [], 'includeUserAgentShadowTree': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(response.result, null, ['documentURL', 'baseURL', 'frameId', 'backendNodeId', 'layoutTreeNodes', 'computedStyles']);
  testRunner.completeTest();
})
