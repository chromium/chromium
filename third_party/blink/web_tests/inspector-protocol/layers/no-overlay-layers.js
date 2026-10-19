// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      .layer {
          transform: translateZ(10px);
          opacity: 0.8;
          background: blue;
      }
      </style>
      <div id="a" style="width: 200px; height: 200px" class="layer">
        CONTENT
      </div>
    `,
      `Tests that Overlay.highlightRect layers are not included in LayerTree.layerTreeDidChange.`);

  await session.evaluate(`
      function updateGeometry()
      {
          document.getElementById("a").style.width = "300px";
      }
  `);

  const initialLayersPromise = dp.LayerTree.onceLayerTreeDidChange();
  await dp.LayerTree.enable();
  const initialLayers = (await initialLayersPromise).params.layers;
  const layersBeforeHighlight =
      initialLayers.map(layer => layer.layerId).sort();

  await dp.DOM.enable();
  await dp.Overlay.enable();
  await dp.Overlay.highlightRect(
      {x: 0, y: 0, width: 200, height: 200, color: {r: 255, g: 0, b: 0}});

  const updatedLayersPromise = dp.LayerTree.onceLayerTreeDidChange();
  await session.evaluate('updateGeometry()');
  const updatedLayers = (await updatedLayersPromise).params.layers;
  const layersAfterHighlight = updatedLayers.map(layer => layer.layerId).sort();

  if (JSON.stringify(layersBeforeHighlight) !==
      JSON.stringify(layersAfterHighlight)) {
    testRunner.log('FAIL: layers changed after highlightRect');
    testRunner.log({layersBeforeHighlight, layersAfterHighlight});
  } else {
    testRunner.log('DONE');
  }
  testRunner.completeTest();
});
