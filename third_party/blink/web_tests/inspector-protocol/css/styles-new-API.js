// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
<head>
<link rel="stylesheet" href="${testRunner.url('resources/styles-new-API.css')}">
<style>

/* An inline stylesheet */
body.mainpage {
    text-decoration: none; /* at least one valid property is necessary for WebCore to match a rule */
    ;badproperty: 1badvalue1;
}

body.mainpage {
    prop1: val1;
    prop2: val2;
}

body:hover {
  color: #CDE;
}

#target:target {
  background: #bada55;
  outline: 5px solid lime;
}
</style>
</head>
<body id="mainBody" class="main1 main2 mainpage" style="font-weight: normal; width: 85%; background-image: url(bar.png)">
  <table width="50%" id="thetable">
  </table>
  <h1 id="toggle">H1</h1>
  <p id="target">:target</p>
</body>
    `,
      'Tests that InspectorCSSAgent API methods work as expected.');

  await session.evaluateAsync(
      'document.readyState === "complete" || new Promise(r => window.addEventListener("load", r))');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.Page.enable();
  await dp.DOM.enable();
  await dp.CSS.enable();

  const frameTreeRes = await dp.Page.getFrameTree();
  const mainFrameId = frameTreeRes.result.frameTree.frame.id;
  const {result: {root}} = await dp.DOM.getDocument({depth: -1});
  const documentNodeId = root.nodeId;

  function findNodeById(node, idAttr) {
    if (Array.isArray(node.attributes)) {
      for (let i = 0; i < node.attributes.length; i += 2) {
        if (node.attributes[i] === 'id' && node.attributes[i + 1] === idAttr) {
          return node;
        }
      }
    }
    for (const child of node.children || []) {
      const found = findNodeById(child, idAttr);
      if (found) {
        return found;
      }
    }
    return null;
  }

  let bodyId;
  testRunner.runTestSuite([
    async function test_styles() {
      bodyId = await cssHelper.requestNodeId(documentNodeId, '#mainBody');
      const [{result: {computedStyle}}, {result: matched}] = await Promise.all([
        dp.CSS.getComputedStyleForNode({nodeId: bodyId}),
        dp.CSS.getMatchedStylesForNode({nodeId: bodyId}),
      ]);

      testRunner.log('');
      testRunner.log('=== Computed style property count for body ===');
      const propCount = computedStyle.length;
      testRunner.log(propCount > 200 ? 'OK' : 'FAIL (' + propCount + ')');

      testRunner.log('');
      testRunner.log('=== Matched rules for body ===');
      cssHelper.dumpRuleMatchesArray(matched.matchedCSSRules);

      testRunner.log('');
      testRunner.log('=== Pseudo rules for body ===');
      for (let i = 0; i < matched.pseudoElements.length; ++i) {
        testRunner.log('PseudoType=' + matched.pseudoElements[i].pseudoType);
        cssHelper.dumpRuleMatchesArray(matched.pseudoElements[i].matches);
      }

      testRunner.log('');
      testRunner.log('=== Inherited styles for body ===');
      for (let i = 0; i < matched.inherited.length; ++i) {
        testRunner.log('Level=' + (i + 1));
        cssHelper.dumpStyleWithRanges(matched.inherited[i].inlineStyle);
        cssHelper.dumpRuleMatchesArray(matched.inherited[i].matchedCSSRules);
      }

      testRunner.log('');
      testRunner.log('=== Inline style for body ===');
      cssHelper.dumpStyleWithRanges(matched.inlineStyle);
    },

    async function test_forcedStateHover() {
      await dp.CSS.forcePseudoState(
          {nodeId: bodyId, forcedPseudoClasses: ['hover']});
      const {result: response} =
          await dp.CSS.getMatchedStylesForNode({nodeId: bodyId});

      testRunner.log('=== BODY with forced :hover ===');
      cssHelper.dumpRuleMatchesArray(response.matchedCSSRules);
    },

    async function test_forcedStateTarget() {
      await dp.CSS.forcePseudoState(
          {nodeId: bodyId, forcedPseudoClasses: ['target']});
      const nodeId = await cssHelper.requestNodeId(documentNodeId, '#target');
      await dp.CSS.forcePseudoState({nodeId, forcedPseudoClasses: ['target']});
      const {result: response} = await dp.CSS.getMatchedStylesForNode({nodeId});

      testRunner.log('=== #target with forced :target ===');
      cssHelper.dumpRuleMatchesArray(response.matchedCSSRules);

      await dp.CSS.forcePseudoState({nodeId, forcedPseudoClasses: []});
    },

    async function test_textNodeComputedStyles() {
      const toggleNode = findNodeById(root, 'toggle');
      const textNode = toggleNode.children[0];
      const {result: {computedStyle}} =
          await dp.CSS.getComputedStyleForNode({nodeId: textNode.nodeId});
      testRunner.log('');
      testRunner.log('=== Computed style property count for TextNode ===');
      const propCount = computedStyle.length;
      testRunner.log(propCount > 200 ? 'OK' : 'FAIL (' + propCount + ')');
    },

    async function test_tableStyles() {
      const tableNodeId =
          await cssHelper.requestNodeId(documentNodeId, '#thetable');
      const {result: response} =
          await dp.CSS.getInlineStylesForNode({nodeId: tableNodeId});
      testRunner.log('');
      testRunner.log('=== Attributes style for table ===');
      cssHelper.dumpStyleWithRanges(response.attributesStyle);

      const {result} = await dp.CSS.getStyleSheetText(
          {styleSheetId: response.inlineStyle.styleSheetId});
      testRunner.log('');
      testRunner.log('=== Stylesheet-for-inline-style text ===');
      testRunner.log(result.text || '');

      await dp.CSS.setStyleSheetText(
          {styleSheetId: response.inlineStyle.styleSheetId, text: ''});
      testRunner.log('');
      testRunner.log('=== Stylesheet-for-inline-style modification result ===');
      testRunner.log('null');
    },

    async function test_addRule() {
      const {result: {styleSheetId}} =
          await dp.CSS.createStyleSheet({frameId: mainFrameId});
      const range = {startLine: 0, startColumn: 0, endLine: 0, endColumn: 0};
      const {result: {rule}} = await dp.CSS.addRule(
          {styleSheetId, ruleText: 'body {}', location: range});

      await dp.CSS.setStyleTexts({
        edits: [{
          styleSheetId: rule.style.styleSheetId,
          range: {
            startLine: rule.style.range.startLine,
            startColumn: rule.style.range.startColumn,
            endLine: rule.style.range.startLine,
            endColumn: rule.style.range.startColumn,
          },
          text: 'font-family: serif;',
        }],
      });

      const {result: response} =
          await dp.CSS.getMatchedStylesForNode({nodeId: bodyId});
      testRunner.log('');
      testRunner.log('=== Matched rules after rule added ===');
      cssHelper.dumpRuleMatchesArray(response.matchedCSSRules);
    },
  ]);
});
