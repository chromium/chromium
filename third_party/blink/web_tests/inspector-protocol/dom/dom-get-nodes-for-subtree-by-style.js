(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <style>
      .grid {
        display: grid;
      }
      .inline-grid {
        display: inline-grid;
      }
    </style>
    <div>
      <div class="grid"></div>
    </div>
    <div class="not-grid">
      <div class="inline-grid">
      </div>
    </div>
  `, 'Tests finding DOM nodes by computed styles.');

  await dp.DOM.enable();
  const response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  let nodesResponse = await dp.DOM.getNodesForSubtreeByStyle({
    nodeId: rootNodeId,
    computedStyles: [
      { name: 'display', value: 'grid' },
      { name: 'display', value: 'inline-grid' },
    ],
  });

  testRunner.log("Expected nodeIds length: 2");
  testRunner.log("Actual nodeIds length: " + nodesResponse.result.nodeIds.length);

  testRunner.log("Nodes:");
  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse.result);
  }

  testRunner.completeTest();
})

