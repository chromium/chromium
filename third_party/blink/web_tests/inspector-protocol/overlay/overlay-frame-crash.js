// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<div id='foo'>foo</div>`,
      'Regression test for crbug.com/1058718: inspecting and highlighting a node and then running GC must not crash.');

  await dp.DOM.enable();
  await dp.Overlay.enable();
  const {root} = (await dp.DOM.getDocument()).result;
  const {nodeId} =
      (await dp.DOM.querySelector({nodeId: root.nodeId, selector: '#foo'}))
          .result;

  // Selecting a node in the Elements panel inspects and highlights it, which
  // creates the overlay page.
  await dp.DOM.setInspectedNode({nodeId});
  await dp.Overlay.highlightNode(
      {nodeId, highlightConfig: {contentColor: {r: 0, g: 0, b: 255, a: 0.5}}});

  await dp.HeapProfiler.collectGarbage();
  await session.evaluate('1 + 1');

  // Pass if it does not crash.
  testRunner.log('Did not crash.');
  testRunner.completeTest();
});
