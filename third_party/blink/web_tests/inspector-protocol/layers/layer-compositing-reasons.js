// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Tests layer compositing reasons');

  await dp.Runtime.enable();
  dp.Runtime.onExceptionThrown(event => {
    testRunner.log(`FAIL: uncaught exception: ${JSON.stringify(event.params)}`);
    testRunner.completeTest();
  });

  await session.navigate('resources/compositing-reasons.html');

  await dp.DOM.enable();
  await dp.DOM.getDocument();
  const layerTreeChangedPromise = dp.LayerTree.onceLayerTreeDidChange();
  await dp.LayerTree.enable();
  const {layers} = (await layerTreeChangedPromise).params;

  const uniqueBackendNodeIds = Array.from(
      new Set(layers.map(layer => layer.backendNodeId).filter(Boolean)));
  const pushResp = await dp.DOM.pushNodesByBackendIdsToFrontend(
      {backendNodeIds: uniqueBackendNodeIds});
  if (pushResp.error) {
    testRunner.log(`FAIL: pushNodesByBackendIdsToFrontend: ${
        JSON.stringify(pushResp.error)}`);
    testRunner.completeTest();
    return;
  }

  const layerByIdAttr = new Map();
  const labelByLayerId = new Map();
  for (const layer of layers) {
    if (!layer.backendNodeId)
      continue;
    const descResp =
        await dp.DOM.describeNode({backendNodeId: layer.backendNodeId});
    if (descResp.error) {
      testRunner.log(
          `FAIL: describeNode error: ${JSON.stringify(descResp.error)}`);
      continue;
    }
    const node = descResp.result.node;
    const attrs = node.attributes || [];
    let id = '';
    for (let i = 0; i + 1 < attrs.length; i += 2) {
      if (attrs[i] === 'id') {
        id = attrs[i + 1];
        break;
      }
    }
    if (id && !layerByIdAttr.has(id)) {
      layerByIdAttr.set(id, layer);
      labelByLayerId.set(layer.layerId, `${node.localName}#${id}`);
    }
  }

  const idsToTest = [
    'transform3d',
    'scale3d',
    'rotate3d',
    'translate3d',
    'backface-visibility',
    'animation',
    'animation-scale',
    'animation-rotate',
    'animation-translate',
    'transformWithCompositedDescendants',
    'transformWithCompositedDescendants-individual',
    'opacityWithCompositedDescendants',
    'reflectionWithCompositedDescendants',
    'perspective',
    'preserve3d',
    'scrollOpaque',
    'scrollForced',
  ];

  for (const id of idsToTest) {
    const layer = layerByIdAttr.get(id);
    if (!layer) {
      testRunner.log(`ERROR: No layer for ${id}`);
      continue;
    }
    const response =
        await dp.LayerTree.compositingReasons({layerId: layer.layerId});
    if (response.error) {
      testRunner.log(`FAIL: compositingReasons error for ${id}: ${
          JSON.stringify(response.error)}`);
      continue;
    }
    const reasonIds = response.result.compositingReasonIds;
    testRunner.log(
        `Compositing reason ids for ${labelByLayerId.get(layer.layerId)}: ` +
        reasonIds.slice().sort().join(','));
  }

  await dp.LayerTree.disable();
  testRunner.completeTest();
});
