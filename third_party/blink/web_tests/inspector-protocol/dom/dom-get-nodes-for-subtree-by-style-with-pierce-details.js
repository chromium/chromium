(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <details>
      <summary>
        Summary
        <div class="in-summary" style="display: grid;"></div>
      </summary>
      Details
      <div class="in-details" style="display: grid;"></div>
    </details>
    <div class="outside" style="display: grid;"></div>
    `, 'Tests finding DOM nodes by computed styles on a page containing <details> elements.');

  await dp.DOM.enable();
  const response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  const nodesResponse = await dp.DOM.getNodesForSubtreeByStyle({
    nodeId: rootNodeId,
    pierce: true,
    computedStyles: [
      { name: 'display', value: 'grid' },
    ],
  });

  testRunner.log('Expected nodeIds length: 3');
  testRunner.log('Actual nodeIds length: ' + nodesResponse.result.nodeIds.length);

  testRunner.log('Nodes:');
  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse.result);
  }

  testRunner.completeTest();
})

