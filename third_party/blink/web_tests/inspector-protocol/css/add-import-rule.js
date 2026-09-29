// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <!DOCTYPE html>
      <style>span { color: red }</style>
      <span id="styled-span"></span>
    `,
      'Tests that adding an @import with data URI does not lead to stylesheet collection crbug.com/644719');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const styleSheetHeaders = new Map();
  dp.CSS.onStyleSheetAdded(({params: {header}}) => {
    styleSheetHeaders.set(header.styleSheetId, header);
  });

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#styled-span');
  const {result: matchedBefore} =
      await dp.CSS.getMatchedStylesForNode({nodeId});
  const sheetId =
      matchedBefore.matchedCSSRules.find(m => m.rule.origin === 'regular')
          .rule.style.styleSheetId;

  testRunner.log('\n== Matched rules before @import added ==\n');
  await cssHelper.dumpSelectedNodeStyles(nodeId, false, styleSheetHeaders);

  const addedPromise = dp.CSS.onceStyleSheetAdded();
  await dp.CSS.setStyleSheetText({
    styleSheetId: sheetId,
    text: '@import \'data:text/css,span{color:green}\';'
  });
  await addedPromise;

  testRunner.log('\n== Matched rules after @import added ==\n');
  await cssHelper.dumpSelectedNodeStyles(nodeId, false, styleSheetHeaders);
  testRunner.completeTest();
});
