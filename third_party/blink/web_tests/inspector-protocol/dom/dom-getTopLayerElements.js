(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {page, session, dp} = await testRunner.startURL('resources/dom-get-top-layer-elements.html', 'Tests DOM.getTopLayerElements command.');

  await dp.DOM.enable();
  await dp.DOM.getDocument();
  await dp.Runtime.evaluate({ expression: `document.getElementById('dialogButton1').click()`});
  await dp.Runtime.evaluate({ expression: `document.getElementById('dialogButton2').click()`});
  await dp.Runtime.evaluate({ expression: `
    const iframe = document.querySelector('iframe');
    iframe.contentWindow.iframeDialog.showModal();
  `});

  const nodesResponse = await dp.DOM.getTopLayerElements();

  testRunner.log("Nodes:");

  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse);
  }
  testRunner.completeTest();
})
