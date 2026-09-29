// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <link rel="stylesheet" href="${
          testRunner.url('resources/import-pseudoclass-crash.css')}">
      <div>
          <p id="lastchild">:last-child</p>
      </div>
    `,
      'Tests that modifying stylesheet text with @import and :last-child selector does not crash (Bug 95324).');

  await session.evaluateAsync(
      'document.readyState === "complete" || new Promise(r => window.addEventListener("load", r))');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#lastchild');
  const {result: matched} = await dp.CSS.getMatchedStylesForNode({nodeId});
  const styleSheetId =
      matched.matchedCSSRules.find(m => m.rule.origin === 'regular')
          .rule.style.styleSheetId;

  await dp.CSS.setStyleSheetText({
    styleSheetId,
    text:
        '@import url("import-pseudoclass-crash-empty.css");\n\n:last-child { color: #000001; }\n',
  });
  await dp.CSS.setStyleSheetText({
    styleSheetId,
    text:
        '@import url("import-pseudoclass-crash-empty.css");\n\n:last-child { color: #002001; }\n',
  });

  testRunner.completeTest();
});
