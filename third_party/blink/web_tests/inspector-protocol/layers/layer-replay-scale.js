// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div id="a" style="background-color:blue; will-change: transform; overflow: hidden;">
        <div style="width:50px; height:50px; background-color:red;"></div>
        <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABMAAAAVCAYAAACkCdXRAAAABHNCSVQICAgIfAhkiAAAACJJREFUOI1j/P///38GKgEmahk0atioYaOGjRo2atiQMwwArTMEJmNxHGkAAAAASUVORK5CYII=">
        <svg>
          <rect x="0" y="0" width="10" height="10" style="opacity:0.5"/>
        </svg>
      </div>
      `,
      'Tests that when layer snapshots are replayed with scaling applied the image dimensions are properly scaled.');

  await session.evaluateAsync(async () => {
    const a = document.getElementById('a');
    const size = (180 / window.devicePixelRatio) + 'px';
    a.style.width = size;
    a.style.height = size;
    await a.querySelector('img').decode();
  });

  await dp.DOM.enable();
  const {root} = (await dp.DOM.getDocument()).result;
  const {nodeId} = (await dp.DOM.querySelector({
                     nodeId: root.nodeId,
                     selector: '#a',
                   })).result;
  const {node} = (await dp.DOM.describeNode({nodeId})).result;
  const backendNodeId = node.backendNodeId;

  const isTargetLayer = layer => layer.backendNodeId === backendNodeId &&
      layer.width > 0 && layer.height > 0;
  const layerTreeChangedPromise = dp.LayerTree.onceLayerTreeDidChange(
      event => (event.params.layers || []).some(isTargetLayer));
  await dp.LayerTree.enable();
  const {layers} = (await layerTreeChangedPromise).params;
  const layer = layers.find(isTargetLayer);

  const {snapshotId} =
      (await dp.LayerTree.makeSnapshot({layerId: layer.layerId})).result;
  await testImageForSnapshot(snapshotId, undefined);
  await testImageForSnapshot(snapshotId, 0.5);
  await dp.LayerTree.releaseSnapshot({snapshotId});
  testRunner.completeTest();

  async function testImageForSnapshot(snapshotId, scale) {
    const {dataURL} =
        (await dp.LayerTree.replaySnapshot({snapshotId, scale})).result;
    const dimensions = await session.evaluateAsync(async imageURL => {
      const image = new Image();
      await new Promise((resolve, reject) => {
        image.addEventListener('load', resolve, false);
        image.addEventListener(
            'error', () => reject(new Error('Failed to load snapshot image')),
            false);
        image.src = imageURL;
      });
      return `${image.naturalWidth} x ${image.naturalHeight}`;
    }, dataURL);
    testRunner.log(`Image dimensions at scale ${scale}: ${dimensions}`);
  }
});
