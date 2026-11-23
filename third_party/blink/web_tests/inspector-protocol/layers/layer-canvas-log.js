// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
      <div id="a" style="will-change: transform; background-color:blue; width:100px; height:100px;">
          <div style="width:50px; height:50px; background-color:red;"></div>
          <img src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABMAAAAVCAYAAACkCdXRAAAAAXNSR0IArs4c6QAAACpJREFUOE9j/P//fwMDlQDjqGEkh+RomJEcZAyjYTYaZjhCYDRpDHDSAAAQh0lXtDaFqwAAAABJRU5ErkJggg==">
          <svg>
              <rect x="0" y="0" width="10" height="10" style="opacity:0.5"/>
          </svg>
      </div>
  `,
                                                   'Tests layer command log\n');

  await session.evaluateAsync(async () => {
    const img = document.querySelector('#a img');
    if (!img.complete) {
      await new Promise((resolve, reject) => {
        img.onload = resolve;
        img.onerror = () => reject(new Error('Image failed to load'));
      });
    }
    await img.decode();
    await new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
  });

  await dp.DOM.enable();
  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} = await dp.DOM.querySelector({
    nodeId: root.nodeId,
    selector: '#a',
  });
  const {result: {node: {backendNodeId}}} = await dp.DOM.describeNode({nodeId});

  const layersPromise = dp.LayerTree.onceLayerTreeDidChange(
      event => event.params.layers.some(layer => layer.backendNodeId ===
                                            backendNodeId));
  await dp.LayerTree.enable();
  const {params: {layers}} = await layersPromise;

  const targetLayer =
      layers.find(layer => layer.backendNodeId === backendNodeId);
  if (!targetLayer) {
    testRunner.log('ERROR: No layer for a');
    testRunner.completeTest();
    return;
  }

  const {result: {snapshotId}} = await dp.LayerTree.makeSnapshot({
    layerId: targetLayer.layerId,
  });
  if (!snapshotId) {
    testRunner.log('ERROR: Empty snapshotId from LayerTree.makeSnapshot');
    testRunner.completeTest();
    return;
  }

  const {result: {commandLog}} = await dp.LayerTree.snapshotCommandLog({
    snapshotId,
  });

  function stripUniqueId(value) {
    if (Array.isArray(value)) {
      return value.map(stripUniqueId);
    }
    if (value && typeof value === 'object') {
      const cleaned = {};
      for (const [k, v] of Object.entries(value)) {
        if (k === 'uniqueID') {
          continue;
        }
        cleaned[k] = stripUniqueId(v);
      }
      return cleaned;
    }
    return value;
  }

  const items = commandLog.map((entry, commandIndex) => ({
                                 commandIndex,
                                 method: entry.method,
                                 params: stripUniqueId(entry.params),
                               }));

  testRunner.log('Canvas log:');
  testRunner.log(items);
  testRunner.completeTest();
});
