(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startURL(
    '../resources/dom-get-querying-descendants-for-container.html',
    'Test CSS.getQueryingDescendantsForContainer method for container query containers');
  await dp.DOM.enable();

  const documentResponse = await dp.DOM.getDocument();
  const documentNodeId = documentResponse.result.root.nodeId;

  const testQueryingDescendantsResult = async (containerSelector, descendantsSelector) => {
    // Test unnamed container query descendants.
    const containerSelectorResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: containerSelector});
    const containerNodeId = containerSelectorResponse.result.nodeId;

    const descendantsQuerySelectorResponse = await dp.DOM.querySelectorAll({nodeId: documentNodeId, selector: descendantsSelector});
    const expectedDescendants = new Set(descendantsQuerySelectorResponse.result.nodeIds);

    const getQueryingDescendantsResponse = await dp.DOM.getQueryingDescendantsForContainer({
      nodeId: containerNodeId,
    });

    const queryingDescendants = getQueryingDescendantsResponse.result.nodeIds;
    if (!queryingDescendants) {
      testRunner.completeTest();
      return;
    }

    testRunner.log(`Are the returned nodes the expected descendants for ${containerSelector}?`);
    testRunner.log(
      queryingDescendants.length === expectedDescendants.size &&
      queryingDescendants.every(nodeId => expectedDescendants.has(nodeId))
    );
  }

  await testQueryingDescendantsResult('#container-unnamed', '.desc-unnamed');
  await testQueryingDescendantsResult('#container-named', '.desc-named');
  await testQueryingDescendantsResult('#container-unnamed-style', '.desc-unnamed-style');
  await testQueryingDescendantsResult('#container-named-style', '.desc-named-style');

  testRunner.completeTest();
});
