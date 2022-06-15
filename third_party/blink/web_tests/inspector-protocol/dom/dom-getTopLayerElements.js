(async function(testRunner) {
    const {page, session, dp} = await testRunner.startHTML(`
    <button id="dialogButton1" onclick="window.dialog1.showModal();">Open Dialog</button>
    <dialog id="dialog1">
        <button id="dialogButton2" onclick="window.dialog2.showModal();">Open Dialog</button>
    </dialog>
    <dialog id="dialog2"></dialog>
  `, 'Tests DOM.getTopLayerElements command.');

  await dp.DOM.enable();
  await dp.DOM.getDocument();
  await dp.Runtime.evaluate({ expression: `document.getElementById('dialogButton1').click()`});
  await dp.Runtime.evaluate({ expression: `document.getElementById('dialogButton2').click()`});

  const nodesResponse = await dp.DOM.getTopLayerElements();

  testRunner.log("Nodes:");

  for (const nodeId of nodesResponse.result.nodeIds) {
    const nodeResponse = await dp.DOM.describeNode({ nodeId });
    testRunner.log(nodeResponse);
  }
  testRunner.completeTest();
})