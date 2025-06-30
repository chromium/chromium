(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('resources/dom-perform-search.html', 'Tests DOM.performSearch command.');
  await dp.DOM.getDocument();

  let performSearchResult = await dp.DOM.performSearch({query: '.find-me'});
  let nodesResponse = await dp.DOM.getSearchResults({searchId: performSearchResult.result.searchId, fromIndex: 0, toIndex: 4});

  testRunner.log("Nodes:");

  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse);
  }

  performSearchResult = await dp.DOM.performSearch({query: 'html'});
  nodesResponse = await dp.DOM.getSearchResults({searchId: performSearchResult.result.searchId, fromIndex: 0, toIndex: 2});

  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse);
  }
  testRunner.completeTest();
})
