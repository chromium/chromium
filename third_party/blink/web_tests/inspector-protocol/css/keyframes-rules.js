// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <style>
      @keyframes fadeout {
          from { background-color: black; }
          to { background-color: white; }
      }

      * {
          background-color: papayawhip;
      }
      </style>
      <div id="inspected" style="background-color: white;">Content</div>
    `,
      'Tests that source data is extracted correctly from stylesheets with @keyframes rules.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');

  testRunner.runTestSuite([
    async function testInit() {
      await dp.CSS.getMatchedStylesForNode({nodeId});
    },

    async function testDumpStyles() {
      await cssHelper.dumpSelectedNodeStyles(nodeId);
    },
  ]);
});
