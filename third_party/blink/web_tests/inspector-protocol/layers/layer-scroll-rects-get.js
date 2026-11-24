// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div style="transform: translateZ(100px);height:20px;width:30px;" onmousewheel=""></div>
      <div id="touchable" style="transform:translateZ(100px);height:20px;width:20px;overflow:scroll;">
        <div style="height:40px;width:40px;"></div>
      </div>
      `,
      'Tests scroll rectangles reported by LayerTree.layerTreeDidChange.\n');

  await session.evaluateAsync(() => {
    const element = document.getElementById('touchable');
    element.addEventListener('touchstart', () => {}, false);
    return new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
  });

  const layerTreeChangedPromise = dp.LayerTree.onceLayerTreeDidChange();
  await dp.LayerTree.enable();
  const {params: {layers}} = await layerTreeChangedPromise;

  testRunner.log('Scroll rectangles');
  for (const layer of layers) {
    if (layer.scrollRects?.length > 0) {
      testRunner.log(layer.scrollRects);
    }
  }
  testRunner.completeTest();
});
