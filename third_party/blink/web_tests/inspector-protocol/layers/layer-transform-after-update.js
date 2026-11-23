// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
        #layer {
          will-change: transform;
          width: 300px;
          height: 300px;
          transform: translate(100px, 100px);
        }
      </style>
      <div id="layer"></div>
  `,
      'Tests that LayerTree.layerTreeDidChange preserves layer transform and geometry after updating backfaceVisibility.');

  function extractLayerGeometry(layers, backendNodeId) {
    const matches =
        (layers || []).filter(l => l.backendNodeId === backendNodeId);
    if (matches.length !== 1) {
      testRunner.fail(`Expected 1 layer for #layer (backendNodeId=${
          backendNodeId}), got ${matches.length}`);
      return null;
    }
    const l = matches[0];
    const parent = l.parentLayerId ?
        layers.find(p => p.layerId === l.parentLayerId) :
        null;
    return {
      offsetX: l.offsetX,
      offsetY: l.offsetY,
      width: l.width,
      height: l.height,
      transform: l.transform || null,
      anchorX: l.anchorX ?? 0,
      anchorY: l.anchorY ?? 0,
      anchorZ: l.anchorZ ?? 0,
      parentOffsetX: parent ? parent.offsetX : null,
      parentOffsetY: parent ? parent.offsetY : null,
      parentTransform: parent ? (parent.transform || null) : null,
    };
  }

  const {result: {result: layerObj}} = await dp.Runtime.evaluate({
    expression: `document.getElementById('layer')`,
  });
  const {result: {node: {backendNodeId}}} = await dp.DOM.describeNode({
    objectId: layerObj.objectId,
  });

  const initialLayersPromise = dp.LayerTree.onceLayerTreeDidChange();
  await dp.LayerTree.enable();
  const initialLayers = (await initialLayersPromise).params.layers;
  const initialGeometry = extractLayerGeometry(initialLayers, backendNodeId);
  if (!initialGeometry) {
    return;
  }
  testRunner.log(initialGeometry, 'Initial layer geometry');

  // Updating layers should not produce invalid layer to-screen transforms
  // (see: https://crbug.com/977578). Backface visibility is changed, rather
  // than just adjusting the transform, to ensure fast-path optimizations do not
  // prevent a full layer tree update.
  const updatedLayersPromise = dp.LayerTree.onceLayerTreeDidChange();
  await session.evaluate(
      `document.getElementById('layer').style.backfaceVisibility = 'hidden';`);
  const updatedLayers = (await updatedLayersPromise).params.layers;
  const updatedGeometry = extractLayerGeometry(updatedLayers, backendNodeId);
  if (!updatedGeometry) {
    return;
  }

  const backfaceVisibility = await session.evaluate(
      `getComputedStyle(document.getElementById('layer')).backfaceVisibility`);
  testRunner.log(`backfaceVisibility: ${backfaceVisibility}`);

  if (JSON.stringify(initialGeometry) === JSON.stringify(updatedGeometry)) {
    testRunner.log(
        'Pass: Layer transform and geometry are unchanged by no-op update');
  } else {
    testRunner.log(updatedGeometry, 'Updated layer geometry');
    testRunner.fail('Layer transform or geometry changed by no-op update');
  }

  testRunner.completeTest();
});
