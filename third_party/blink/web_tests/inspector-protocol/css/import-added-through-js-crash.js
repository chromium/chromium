// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      </style>
      <div>
          <p id="inspected"></p>
      </div>
    `,
      'Tests that adding @import rules into a stylesheet through JavaScript does not crash the inspected page.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  await session.evaluate(`
      function addImportRule()
      {
          document.styleSheets[0].insertRule("@import url(${
      testRunner.url('resources/import-added-through-js-crash.css')})", 0);
      }
  `);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');

  testRunner.runTestSuite([
    async function selectNode() {
      await dp.CSS.getMatchedStylesForNode({nodeId});
    },

    async function addImportRules() {
      let addedPromise = dp.CSS.onceStyleSheetAdded();
      await session.evaluate('addImportRule()');
      await addedPromise;
      await dp.CSS.getMatchedStylesForNode({nodeId});

      addedPromise = dp.CSS.onceStyleSheetAdded();
      await session.evaluate('addImportRule()');
      await addedPromise;
      await dp.CSS.getMatchedStylesForNode({nodeId});
    },
  ]);
});
