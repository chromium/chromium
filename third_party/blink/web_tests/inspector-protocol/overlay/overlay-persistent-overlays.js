(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <style>
      #grid {
        display: grid;
      }
    </style>
    <div id="grid"><div>A</div><div>B</div></div>
  `, 'Verifies that Overlay.setShowGridOverlays works together with Overlay.setInspectMode.');

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.Overlay.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);
  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#grid');

  async function getTrackSizeLabels() {
    return await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        const container = document.querySelector('.track-sizes');
        return String(container ? container.children.length : 0);
      })()`);
    });
  }

  await dp.Overlay.setInspectMode({
    mode: 'searchForNode',
    highlightConfig: {},
  });

  testRunner.log('Expected 0 track size labels; actual: ' + await getTrackSizeLabels());

  await dp.Overlay.setShowGridOverlays({
    gridNodeHighlightConfigs: [{
      nodeId,
      gridHighlightConfig: {
        showTrackSizes: true,
      },
    }]
  });

  // Wait for overlay rendering to finish by requesting an animation frame.
  await session.evaluate(() => {
    return new Promise(resolve => requestAnimationFrame(resolve));
  });

  testRunner.log('Expected 3 track size labels; actual: ' + await getTrackSizeLabels());

  testRunner.completeTest();
});
