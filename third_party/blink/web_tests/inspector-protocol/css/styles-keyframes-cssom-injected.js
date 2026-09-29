// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      </style>
      <div id="element" style="animation: injected 1s infinite"></div>
    `,
      'Tests that inspecting keyframes injected via CSSOM doesn\'t crash.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  await session.evaluate(`
      function injectAnimation()
      {
          var styleSheet = document.styleSheets[0];
          styleSheet.insertRule("@keyframes injected { 0% {opacity:0} 100% {opacity:1} }", 0);
      }
      injectAnimation();
  `);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#element');
  await dp.CSS.getMatchedStylesForNode({nodeId});
  testRunner.completeTest();
});
