(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {dp} = await testRunner.startURL('resources/dom-get-detached-iframe.html', 'Tests DOM.getDetachedDomNodes command.');

    await dp.DOM.enable();

    const detachedElements = await dp.DOM.getDetachedDomNodes();
    const detachedResult = detachedElements.result.detachedNodes

    testRunner.log('Amount of Nodes should be 1:');
    testRunner.log(detachedResult.length);

    const treeNode = detachedResult[0].treeNode

    testRunner.log('Verify that the detached iframe has been obtained:');
    testRunner.log(treeNode.nodeName)

    testRunner.completeTest();
  })
