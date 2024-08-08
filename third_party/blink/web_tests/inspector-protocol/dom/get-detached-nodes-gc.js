(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {dp} = await testRunner.startURL('resources/dom-get-detached-dom-nodes-gc.html', 'Tests DOM.getDetachedDomNodes command.');

    await dp.DOM.enable();
    await dp.Overlay.enable();
    await dp.Memory.enable();

    const root = (await dp.DOM.getDocument()).result.root;
    async function HighlightDiv(id) {
        const nodeId = (await dp.DOM.querySelector(
            {nodeId: root.nodeId, selector: id})).result.nodeId;
        await dp.Overlay.highlightNode({
          highlightConfig: {contentColor: {r: 0, g: 128, b: 0, a: 0.5}},
          nodeId: nodeId,
        });
    }

    await HighlightDiv('#a');
    await HighlightDiv('#ab');
    await HighlightDiv('#abc');
    await HighlightDiv('#a');

    await HighlightDiv('#b');
    await HighlightDiv('#bc');
    await HighlightDiv('#bcd');
    await HighlightDiv('#b');

    dp.Memory.simulatePressureNotification('critical');

    const detachedElements = await dp.DOM.getDetachedDomNodes();
    const detachedResult = detachedElements.result.detachedNodes

    testRunner.log('Amount of Nodes should be 0:');
    testRunner.log(detachedResult.length);

    testRunner.completeTest();
  })
