(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <style>
      .grid {
        display: grid;
      }
    </style>
    <div class="grid"></div>
  `, 'Tests errors when finding DOM nodes by computed styles.');

  const wrongNodeId = -1;

  // Expected: error because DOM agent is not enabled.
  testRunner.log(await dp.DOM.getNodesForSubtreeByStyle({
    nodeId: wrongNodeId,
    computedStyles: [{ name: 'display', value: 'grid' }],
  }));

  await dp.DOM.enable();

  // Expected: error because node ID is wrong.
  testRunner.log(await dp.DOM.getNodesForSubtreeByStyle({
    nodeId: wrongNodeId,
    computedStyles: [{ name: 'display', value: 'grid' }],
  }));

  const response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  // Expected: error because the property value is invalid.
  testRunner.log(await dp.DOM.getNodesForSubtreeByStyle({
    nodeId: rootNodeId,
    computedStyles: [{ name: 'display', value: 'grid' }, { name: 'wrong', value: 'grid' }],
  }));

  testRunner.completeTest();
})

