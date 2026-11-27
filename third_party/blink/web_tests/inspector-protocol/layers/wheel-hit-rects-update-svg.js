// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <div id="svg-root">
      <div>
        <svg width="100" height="100">
          <rect width="50" height="50" id="rect"></rect>
        </svg>
      </div>
      <div>
        <svg width="100" height="100">
          <image width="20" height="20" id="image" xlink:href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVQIHWP4z8DwHwyBNJDN8B8AQNEG+t5Ik2kAAAAASUVORK5CYII="/>
        </svg>
      </div>
    </div>
    `,
      'Tests correctness of wheel scroll regions produced by SVG elements after parent update\n');

  await dp.Runtime.enable();
  dp.Runtime.onExceptionThrown(({params}) => {
    testRunner.fail(`Uncaught exception: ${params.exceptionDetails.text}`);
  });
  dp.Runtime.onConsoleAPICalled(({params}) => {
    if (params.type === 'error') {
      testRunner.fail(
          `Console error: ${params.args.map(arg => arg.value).join(' ')}`);
    }
  });

  await dp.DOM.enable();

  await session.evaluateAsync(() => {
    const rect = document.getElementById('rect');
    rect.addEventListener('wheel', () => {}, false);
    const image = document.getElementById('image');
    image.addEventListener('wheel', () => {}, false);
    return new Promise(
        resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
  });

  const initialLayerTreePromise = dp.LayerTree.onceLayerTreeDidChange();
  await dp.LayerTree.enable();
  const {params: {layers: initialLayers}} = await initialLayerTreePromise;

  const initialScrollRects =
      initialLayers.flatMap(layer => layer.scrollRects || []);
  if (initialScrollRects.length !== 2 || initialScrollRects[0].rect.x !== 8 ||
      initialScrollRects[0].rect.y !== 8 ||
      initialScrollRects[0].rect.width !== 50 ||
      initialScrollRects[0].rect.height !== 50 ||
      initialScrollRects[0].type !== 'WheelEventHandler' ||
      initialScrollRects[1].rect.x !== 8 ||
      initialScrollRects[1].rect.y !== 108 ||
      initialScrollRects[1].rect.width !== 20 ||
      initialScrollRects[1].rect.height !== 20 ||
      initialScrollRects[1].type !== 'WheelEventHandler') {
    testRunner.fail(`Unexpected pre-transform scroll rects: ${
        JSON.stringify(initialScrollRects)}`);
    return;
  }

  const updatedLayerTreePromise = dp.LayerTree.onceLayerTreeDidChange();
  await session.evaluateAsync(() => {
    const svgRoot = document.getElementById('svg-root');
    svgRoot.style.transform = 'translate(100px)';
    return new Promise(resolve => requestAnimationFrame(resolve));
  });
  const {params: {layers}} = await updatedLayerTreePromise;

  testRunner.log('Scroll rectangles');
  for (const layer of layers) {
    if (layer.scrollRects?.length > 0) {
      testRunner.log(layer.scrollRects);
    }
  }
  testRunner.completeTest();
});
