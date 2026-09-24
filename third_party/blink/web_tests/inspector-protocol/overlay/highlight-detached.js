// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that highlighting a detached node does not crash. crbug.com/958958\n');

  await dp.DOM.enable();
  await dp.Overlay.enable();
  await dp.Runtime.enable();

  const {result: {result: remoteObject}} = await dp.Runtime.evaluate({
    expression: `
      var styleElement = document.createElement('style');
      styleElement.type = 'text/css';
      styleElement.textContent = 'content';
      styleElement.id = 'inspected';
      styleElement;
    `,
  });
  await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.requestNode({objectId: remoteObject.objectId});
  await dp.Overlay.highlightNode({
    nodeId,
    highlightConfig: {
      showInfo: true,
      showRulers: true,
      showExtensionLines: true,
      contentColor: {r: 111, g: 168, b: 220, a: 0.66},
      paddingColor: {r: 147, g: 196, b: 125, a: 0.55},
      borderColor: {r: 255, g: 229, b: 153, a: 0.66},
      marginColor: {r: 246, g: 178, b: 107, a: 0.66},
      eventTargetColor: {r: 255, g: 196, b: 196, a: 0.66},
      shapeColor: {r: 96, g: 82, b: 177, a: 0.8},
      shapeMarginColor: {r: 96, g: 82, b: 127, a: 0.6},
    },
  });
  await dp.Overlay.getHighlightObjectForTest({nodeId});
  testRunner.completeTest();
});
