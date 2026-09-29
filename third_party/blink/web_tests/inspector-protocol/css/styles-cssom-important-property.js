// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
    <style id="mycss"></style>
    <script>mycss.sheet.insertRule("div{color:red !important;}",0);</script>
    <div id="inspected">test</div>
  `,
      'Tests that !important modifier is shown for CSSOM-generated properties.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
  await cssHelper.dumpSelectedNodeStyles(nodeId, true);
  testRunner.completeTest();
});
