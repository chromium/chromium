// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL(
      'resources/dynamic-style-tag.html',
      'Tests that different types of inline styles are correctly disambiguated and their sourceURL is correct.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const styleSheetHeaders = new Map();
  dp.CSS.onStyleSheetAdded(({params: {header}}) => {
    styleSheetHeaders.set(header.styleSheetId, {
      ...header,
      hasSourceURL: Boolean(header.hasSourceURL),
    });
  });

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');

  const styleSheetsWithContent = [];
  for (const header of styleSheetHeaders.values()) {
    const {result: {text}} =
        await dp.CSS.getStyleSheetText({styleSheetId: header.styleSheetId});
    styleSheetsWithContent.push({header, content: text});
  }
  styleSheetsWithContent.sort((a, b) => a.content.localeCompare(b.content));
  for (const {header, content} of styleSheetsWithContent) {
    testRunner.log('Stylesheet added:');
    testRunner.log('  - isInline: ' + header.isInline);
    testRunner.log(
        '  - sourceURL: ' +
        header.sourceURL.substring(header.sourceURL.lastIndexOf('/') + 1));
    testRunner.log('  - hasSourceURL: ' + header.hasSourceURL);
    testRunner.log('  - contents: ' + content);
  }
  await cssHelper.dumpSelectedNodeStyles(nodeId, true, styleSheetHeaders);
  testRunner.completeTest();
});
