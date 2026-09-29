// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
    <style>
      @keyframes animName {
        from { color: green; }
        to { color: lime; }
      }
      #container {
        animation: animName 1000s;
        display: none;
      }
      #element {
        animation: inherit;
      }
    </style>
    <div id="container">
      <div id="element"></div>
    </div>
  `,
      'Tests that keyframes are shown in styles pane inside display:none.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const elementId = await cssHelper.requestNodeId(documentNodeId, '#element');
  testRunner.log('=== #element styles ===');
  await cssHelper.dumpSelectedNodeStyles(elementId);

  const containerId =
      await cssHelper.requestNodeId(documentNodeId, '#container');
  testRunner.log('=== #container styles ===');
  await cssHelper.dumpSelectedNodeStyles(containerId);
  testRunner.completeTest();
});
