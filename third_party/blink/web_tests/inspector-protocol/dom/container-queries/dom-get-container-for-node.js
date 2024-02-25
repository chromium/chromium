(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { session, dp } = await testRunner.startURL(
    '../resources/dom-get-container-for-node.html',
    'Test CSS.getContainerForNode method for node with container queries');

  await dp.DOM.enable();

  const documentResponse = await dp.DOM.getDocument();
  const documentNodeId = documentResponse.result.root.nodeId;

  const namedContainerQuerySelectorResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#named-container' });
  const namedContainerNodeId = namedContainerQuerySelectorResponse.result.nodeId;

  const blockSizeContainerQuerySelectorResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#block-size-container' });
  const blockSizeContainerNodeId = blockSizeContainerQuerySelectorResponse.result.nodeId;

  const unnamedContainerQuerySelectorResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#unnamed-container' });
  const unnamedContainerNodeId = unnamedContainerQuerySelectorResponse.result.nodeId;

  const dynamicContainerQuerySelectorResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#dynamic-container' });
  const dynamicContainerNodeId = dynamicContainerQuerySelectorResponse.result.nodeId;

  const styleContainerQuerySelectorResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#style-container' });
  const styleContainerNodeId = styleContainerQuerySelectorResponse.result.nodeId;

  const itemQuerySelectorResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '.item' });
  const itemNodeId = itemQuerySelectorResponse.result.nodeId;

  const namedContainerResponse = await dp.DOM.getContainerForNode({
    nodeId: itemNodeId,
    containerName: 'container-with-name',
    logicalAxes: 'Inline',
  });
  testRunner.log(namedContainerResponse);
  testRunner.log('Is the returned container the expected named container?');
  testRunner.log(namedContainerResponse.result.nodeId === namedContainerNodeId);

  const unnamedContainerResponse = await dp.DOM.getContainerForNode({
    nodeId: itemNodeId,
    logicalAxes: 'Inline',
  });
  testRunner.log(unnamedContainerResponse);
  testRunner.log('Is the returned container the expected unnamed container?');
  testRunner.log(unnamedContainerResponse.result.nodeId === unnamedContainerNodeId);

  const heightContainerResponse = await dp.DOM.getContainerForNode({
    nodeId: itemNodeId,
    physicalAxes: 'Vertical',
  });
  testRunner.log(heightContainerResponse);
  testRunner.log('Is the returned container the expected block-size (physical) container?');
  testRunner.log(heightContainerResponse.result.nodeId === blockSizeContainerNodeId);

  const blockSizeContainerResponse = await dp.DOM.getContainerForNode({
    nodeId: itemNodeId,
    logicalAxes: 'Block',
  });
  testRunner.log(blockSizeContainerResponse);
  testRunner.log('Is the returned container the expected block-size (logical) container?');
  testRunner.log(blockSizeContainerResponse.result.nodeId === blockSizeContainerNodeId);

  const styleContainerResponse = await dp.DOM.getContainerForNode({
    nodeId: itemNodeId,
  });
  testRunner.log(styleContainerResponse);
  testRunner.log('Is the returned container the expected style container?');
  testRunner.log(styleContainerResponse.result.nodeId === styleContainerNodeId);

  // Dynamically add a closer inline-size container to .item and check if this
  // new container can be returned right away.
  await session.evaluate(`
    const dynamicContainer = document.getElementById('dynamic-container');
    dynamicContainer.style.containerType = 'inline-size';
  `);
  const dynamicContainerResponse = await dp.DOM.getContainerForNode({
    nodeId: itemNodeId,
    logicalAxes: 'Inline',
  });
  testRunner.log(dynamicContainerResponse);
  testRunner.log('Is the returned container the expected dynamic container?');
  testRunner.log(dynamicContainerResponse.result.nodeId === dynamicContainerNodeId);

  // Can query height against vertical writing-mode inline-size container.
  await session.evaluate(`
    const orthogonalContainer = document.getElementById('dynamic-container');
    orthogonalContainer.style.writingMode = 'vertical-rl';
    orthogonalContainer.style.containerType = 'inline-size';
  `);
  const orthogonalContainerResponse = await dp.DOM.getContainerForNode({
    nodeId: itemNodeId,
    physicalAxes: 'Vertical',
  });
  testRunner.log(orthogonalContainerResponse);
  testRunner.log('Is the returned container the expected orthogonal container?');
  testRunner.log(orthogonalContainerResponse.result.nodeId === dynamicContainerNodeId);

  testRunner.completeTest();
});
