// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const url1 = testRunner.url('resources/multiple-imports-edit-crash-1.css');
  const url2 = testRunner.url('resources/multiple-imports-edit-crash-2.css');
  const {session, dp} = await testRunner.startHTML(
      `
    <head>
      <style>
        @import url(${url1});
        @import url(${url2});
        @import url(${url1});
        #inspected {
            color: green;
        }
      </style>
    </head>
    <body>
      <div id="inspected">Text</div>
    </body>
  `,
      'Tests that modifying stylesheet text with multiple @import at-rules does not crash.');

  await session.evaluateAsync(
      'document.readyState === "complete" || new Promise(r => window.addEventListener("load", r))');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const styleSheetHeaders = new Map();
  let initialAddsExpected = 4;
  const initialAdded = [];

  await dp.DOM.enable();

  await new Promise(resolve => {
    const onInitialAdd = ({params: {header}}) => {
      styleSheetHeaders.set(header.styleSheetId, header);
      const name = resourceName(header.sourceURL);
      if (name && !header.isInline) {
        // Don't include the <style> element sheet.
        initialAdded.push(name);
      }
      if (!(--initialAddsExpected)) {
        dp.CSS.offStyleSheetAdded(onInitialAdd);
        initialAdded.sort();
        testRunner.log('Initially added:');
        testRunner.log(initialAdded.join('\n'));
        resolve();
      }
    };
    dp.CSS.onStyleSheetAdded(onInitialAdd);
    dp.CSS.enable();
  });

  let addsExpected = 2;
  let removesExpected = 3;
  const added = [];
  const removed = [];

  dp.CSS.onStyleSheetAdded(({params: {header}}) => {
    styleSheetHeaders.set(header.styleSheetId, header);
    added.push(resourceName(header.sourceURL));
    if (!(--addsExpected)) {
      added.sort();
      testRunner.log('Added:');
      testRunner.log(added.join('\n'));
      testRunner.completeTest();
    }
  });

  dp.CSS.onStyleSheetRemoved(({params: {styleSheetId}}) => {
    const header = styleSheetHeaders.get(styleSheetId);
    removed.push(resourceName(header ? header.sourceURL : ''));
    if (!(--removesExpected)) {
      removed.sort();
      testRunner.log('Removed:');
      testRunner.log(removed.join('\n'));
    }
  });

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
  const {result: matchedResult} =
      await dp.CSS.getMatchedStylesForNode({nodeId});
  const styleSheetId =
      matchedResult.matchedCSSRules[matchedResult.matchedCSSRules.length - 1]
          .rule.style.styleSheetId;

  testRunner.log('Setting stylesheet text...');
  await dp.CSS.setStyleSheetText({
    styleSheetId,
    text: `@import url(${url1});\n@import url(${
        url2});\n#inspected { color: black }\n`,
  });

  function resourceName(url) {
    return url.substring(url.lastIndexOf('/') + 1);
  }
});
