// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <div id="inspected1" style="color: red /* foo: bar;"></div>
      <div id="inspected2" style="color: green; /* foo: bar;"></div>
    `,
      'Tests that CSSParser correctly parses declarations with unterminated comments.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId1 = await cssHelper.requestNodeId(documentNodeId, '#inspected1');
  await cssHelper.dumpSelectedNodeStyles(nodeId1);

  const nodeId2 = await cssHelper.requestNodeId(documentNodeId, '#inspected2');
  await cssHelper.dumpSelectedNodeStyles(nodeId2);
  testRunner.completeTest();
});
