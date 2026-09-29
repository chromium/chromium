// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <style>
      /* c1 */
                               html
        #inspected.bar /* c2 */,
       /* c3 */ b
        /* c4 */  {
          text-decoration: none;
      }
      </style>
      <h1 id="inspected" class="bar">Inspect Me</h1>
    `,
      'Tests that WebInspector.CSSStyleSheet methods work as expected.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
  const response = await dp.CSS.getMatchedStylesForNode({nodeId});
  if (response.error) {
    testRunner.log('Failed to get styles: ' + response.error.message);
    return;
  }
  cssHelper.dumpRuleMatchesArray(response.result.matchedCSSRules);
  testRunner.completeTest();
});
