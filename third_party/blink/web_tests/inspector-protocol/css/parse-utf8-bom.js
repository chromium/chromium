// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <link rel="stylesheet" href="${
          testRunner.url('resources/parse-utf8-bom-main.css')}">
      <h1 id="inspected">
      I'm red.
      </h1>
    `,
      'Tests that source data are extracted correctly from external stylesheets in UTF-8 with BOM. Bug 59322.');

  await session.evaluateAsync(
      'document.readyState === "complete" || new Promise(r => window.addEventListener("load", r))');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const styleSheetHeaders = new Map();
  dp.CSS.onStyleSheetAdded(({params: {header}}) => {
    styleSheetHeaders.set(header.styleSheetId, header);
  });

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
  await cssHelper.dumpSelectedNodeStyles(nodeId, true, styleSheetHeaders);
  testRunner.completeTest();
});
