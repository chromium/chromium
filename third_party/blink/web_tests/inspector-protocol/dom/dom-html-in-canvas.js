// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
      body { margin: 0; }
      canvas { display: block; }
      #nondrawn { width: 100px; }
      [drawable] { width: 100px; height: 100px; background: green; }
    </style>
    <div id="root"></div>
  `,
      'Tests content quads, box model, and inspect-mode hit testing for HTML-in-Canvas.');

  await dp.DOM.enable();
  await dp.Overlay.enable();

  async function setMarkup(html) {
    await session.evaluateAsync(async markup => {
      document.getElementById('root').innerHTML = markup;
      await new Promise(
          r => requestAnimationFrame(() => requestAnimationFrame(r)));
    }, html);
    await dp.DOM.getDocument({depth: -1});
  }

  async function logGeometry(selector) {
    const {result: {root}} = await dp.DOM.getDocument();
    const {result: {nodeId}} =
        await dp.DOM.querySelector({nodeId: root.nodeId, selector});
    const {result: {quads}} = await dp.DOM.getContentQuads({nodeId});
    const {result: {model}} = await dp.DOM.getBoxModel({nodeId});
    testRunner.log(`${selector} contentQuads: ${JSON.stringify(quads)}`);
    testRunner.log(`${selector} boxModel: ${JSON.stringify(model)}`);
  }

  async function describeNode(params) {
    const {result: {node}} = await dp.DOM.describeNode(params);
    const attrs = node.attributes || [];
    let id = '';
    for (let i = 0; i < attrs.length; i += 2) {
      if (attrs[i] === 'id') {
        id = `#${attrs[i + 1]}`;
      }
    }
    return `${node.localName}${id}`;
  }

  async function inspectAt(x, y) {
    await dp.Overlay.setInspectMode(
        {mode: 'searchForNode', highlightConfig: {}});
    const highlightPromise = dp.Overlay.onceNodeHighlightRequested();
    const inspectPromise = dp.Overlay.onceInspectNodeRequested();
    await dp.Input.dispatchMouseEvent({
      type: 'mouseMoved',
      button: 'none',
      buttons: 0,
      x,
      y,
    });
    const highlightEvent = await highlightPromise;
    await dp.Input.dispatchMouseEvent({
      type: 'mousePressed',
      button: 'left',
      buttons: 1,
      clickCount: 1,
      x,
      y,
    });
    await dp.Input.dispatchMouseEvent({
      type: 'mouseReleased',
      button: 'left',
      buttons: 0,
      clickCount: 1,
      x,
      y,
    });
    const inspectEvent = await inspectPromise;
    await dp.Overlay.setInspectMode({mode: 'none'});

    const highlighted =
        await describeNode({nodeId: highlightEvent.params.nodeId});
    const inspected =
        await describeNode({backendNodeId: inspectEvent.params.backendNodeId});
    const {result: {backendNodeId: locBackendNodeId}} =
        await dp.DOM.getNodeForLocation({x, y});
    const atLocation = await describeNode({backendNodeId: locBackendNodeId});
    testRunner.log(
        `inspect(${x}, ${y}) -> highlight: ${highlighted}, inspect: ${
            inspected}, getNodeForLocation: ${atLocation}`);
  }

  testRunner.log('1. Non-drawn ancestor of drawable elements:');
  await setMarkup(`
    <canvas id="canvas" width="300" height="300" content="drawable">
      <div id="nondrawn">
        <div id="drawablea" drawable></div>
        <div id="drawableb" drawable></div>
      </div>
    </canvas>
  `);
  await logGeometry('#nondrawn');
  await inspectAt(50, 50);

  testRunner.log('2. Non-drawn drawable elements:');
  await setMarkup(`
    <canvas id="canvas" width="300" height="300" content="drawable">
      <div id="nondrawn">
        <div id="drawable" drawable>
          content
          <div id="nesteddrawable" drawable></div>
        </div>
      </div>
    </canvas>
  `);
  await logGeometry('#drawable');
  await logGeometry('#nesteddrawable');
  await inspectAt(50, 50);

  testRunner.log('3. Drawn drawable elements:');
  await setMarkup(`
    <canvas id="canvas" width="300" height="300" content="drawable">
      <div id="drawable" drawable>
        content
        <div id="nesteddrawable" drawable></div>
      </div>
    </canvas>
  `);
  await session.evaluateAsync(async () => {
    const canvas = document.getElementById('canvas');
    await new Promise(resolve => {
      canvas.addEventListener('paint', resolve, {once: true});
      canvas.requestPaint();
    });
    const ctx = canvas.getContext('2d');
    ctx.drawElementImage(document.getElementById('drawable'), 20, 30);
    ctx.drawElementImage(document.getElementById('nesteddrawable'), 150, 40);
    await new Promise(
        r => requestAnimationFrame(() => requestAnimationFrame(r)));
  });
  await logGeometry('#drawable');
  await logGeometry('#nesteddrawable');
  await inspectAt(5, 5);
  await inspectAt(70, 80);
  await inspectAt(200, 90);

  testRunner.log('4. Manual geometry update via updateElementGeometry:');
  await setMarkup(`
    <canvas id="canvas" width="300" height="300" content="drawable">
      <div id="drawable" drawable>
        content
        <div id="nesteddrawable" drawable></div>
      </div>
    </canvas>
  `);
  await session.evaluateAsync(async () => {
    const canvas = document.getElementById('canvas');
    canvas.updateElementGeometry(document.getElementById('drawable'), {
      canvasTransform: new DOMMatrix().translate(10, 20).scale(1, 2),
      canvasClip: {x: 0, y: 0, width: 100, height: 60},
    });
    canvas.updateElementGeometry(document.getElementById('nesteddrawable'), {
      canvasTransform: new DOMMatrix().translate(140, 30),
    });
    await new Promise(
        r => requestAnimationFrame(() => requestAnimationFrame(r)));
  });
  await logGeometry('#drawable');
  await logGeometry('#nesteddrawable');
  await inspectAt(5, 5);
  await inspectAt(60, 80);
  await inspectAt(60, 180);
  await inspectAt(190, 80);

  testRunner.completeTest();
});
