// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL(
      'resources/styles-source-lines-recovery-helper.html',
      'Tests that invalid rule inside @-rule doesn\'t break source code matching (http://crbug.com/317499).');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const styleSheetHeaders = new Map();
  dp.CSS.onStyleSheetAdded(({params: {header}}) => {
    styleSheetHeaders.set(header.styleSheetId, header);
  });

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#main');
  await cssHelper.dumpSelectedNodeStyles(nodeId, true, styleSheetHeaders);
  testRunner.completeTest();
});
