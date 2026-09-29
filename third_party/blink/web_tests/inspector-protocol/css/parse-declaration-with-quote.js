// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <div id="inspected" style="color: red ' foo"></div>
    `,
      'Tests that CSSParser correctly parses declarations with unterminated strings. Blink bug 231127');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
  await cssHelper.dumpSelectedNodeStyles(nodeId);
  testRunner.completeTest();
});
