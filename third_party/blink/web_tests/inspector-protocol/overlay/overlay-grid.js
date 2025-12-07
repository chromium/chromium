(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<div style="display: grid; grid-template-columns: auto auto;">
        <div id=subgrid style="display: grid; grid-template-columns: subgrid [a] [b] [c];">`,
      'Test that showing the grid overlay with line names does not crash for out-of-bound line specs');

  await dp.DOM.enable();
  await dp.Overlay.enable();

  const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
  const {result: {nodeId}} = await dp.DOM.querySelector(
      {nodeId: documentNodeId, selector: '#subgrid'});

  await dp.Overlay.setShowGridOverlays({
    gridNodeHighlightConfigs:
        [{gridHighlightConfig: {showLineNames: true}, nodeId}]
  });

  await (dp.Runtime.evaluate({
    expression: 'new Promise(r => requestAnimationFrame(r))',
    awaitPromise: true
  }));
  testRunner.log('did not crash');
  testRunner.completeTest();
});
