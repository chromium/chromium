// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      div {
          border: 1px solid black;
          background-color: white;
          padding: 20px;
      }
      </style>
      <div id="box">Inspecting this element crashes DevTools</div>
    `,
      'Tests that the inspected page does not crash after inspecting element with CSSOM added rules. Bug 373508 crbug.com/373508');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  await session.evaluate(`
      var lastSheet = document.styleSheets[document.styleSheets.length - 1];
      var mediaIndex = lastSheet.insertRule('@media all { }', lastSheet.cssRules.length);
      var mediaRule = lastSheet.cssRules[mediaIndex];
      mediaRule.insertRule('#box { background: red; color: white; }', mediaRule.cssRules.length);
    `);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#box');
  await cssHelper.dumpSelectedNodeStyles(nodeId, true);
  testRunner.completeTest();
});
