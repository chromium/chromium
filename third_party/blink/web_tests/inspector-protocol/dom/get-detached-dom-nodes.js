(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {dp} = await testRunner.startURL('resources/dom-get-detached-dom-nodes.html', 'Tests DOM.getDetachedDomNodes command.');

    await dp.DOM.enable();

    const detachedElements = await dp.DOM.getDetachedDomNodes();
    const detachedResult = detachedElements.result.detachedNodes

    testRunner.log('Amount of Nodes should be 1:');
    testRunner.log(detachedResult.length);

    const treeNode = detachedResult[0].treeNode
    const detachedIDs = detachedResult[0].retainedNodeIds

    testRunner.log('Verify Children Count:');
    testRunner.log(treeNode.children.length == treeNode.childNodeCount);
    testRunner.log(treeNode.childNodeCount);

    testRunner.log("Verify Retained IDs match")
    testRunner.log(detachedIDs[0] == treeNode.children[0].backendNodeId)
    testRunner.log(detachedIDs[1] == treeNode.children[1].backendNodeId)
    testRunner.log("Verify we aren't getting every child, but every detached node")
    testRunner.log(detachedIDs.length)

    testRunner.log('Verify that the return node is the Top Level Node: Detached Paragraph:');
    testRunner.log(treeNode.attributes[1])
    testRunner.log(treeNode.nodeName)

    testRunner.completeTest();
  })
