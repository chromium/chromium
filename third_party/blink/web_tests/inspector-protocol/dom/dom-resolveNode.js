(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
    '', 'Tests that DOM.resolveNode accepts a nodeId or backendNodeId.');

  dp.DOM.enable();
  var response = await dp.DOM.getDocument({depth: 0});
  if (!response.result)
    testRunner.log(JSON.stringify(response));
  var nodeId = response.result.root.nodeId;
  var backendNodeId = response.result.root.backendNodeId;

  response = await dp.DOM.resolveNode({nodeId: nodeId});
  if (!response.result || !response.result.object.description)
    testRunner.log(JSON.stringify(response));
  testRunner.log("description: " + response.result.object.description);

  response = await dp.DOM.resolveNode({backendNodeId: backendNodeId});
  if (!response.result || !response.result.object.description)
    testRunner.log(JSON.stringify(response));
  testRunner.log("description: " + response.result.object.description);

  testRunner.completeTest();
})
