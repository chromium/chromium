// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
<head>
<link rel="stylesheet" href="${
          testRunner.url('resources/styles-source-offsets.css')}">
<style>

body.mainpage {
    text-decoration: none; /* at least one valid property is necessary for WebCore to match a rule */
    badproperty: 1badvalue1;
}

</style>
</head>
<body id="mainBody" class="main1 main2 mainpage" style="font-weight: normal; width: 80%">
</body>
    `,
      'Tests that proper data and start/end offset positions are reported for CSS style declarations and properties.');

  await session.evaluateAsync(
      'document.readyState === "complete" || new Promise(r => window.addEventListener("load", r))');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  function dumpStyleData(ruleOrStyle) {
    const isRule = !!(ruleOrStyle.style);
    let style;
    let header = '';
    if (isRule) {
      if (ruleOrStyle.origin !== 'regular') {
        return;
      }
      style = ruleOrStyle.style;
      const selectors = ruleOrStyle.selectorList.selectors;
      const firstRange = selectors[0].range;
      const lastRange = selectors[selectors.length - 1].range;
      const range = {
        startLine: firstRange.startLine,
        startColumn: firstRange.startColumn,
        endLine: lastRange.endLine,
        endColumn: lastRange.endColumn,
      };
      header = ruleOrStyle.selectorList.text + ': ' +
          (range ? cssHelper.rangeText(range) : '');
    } else {
      style = ruleOrStyle;
      header = 'element.style:';
    }
    testRunner.log(header + ' ' + cssHelper.rangeText(style.range));
    const allProperties = style.cssProperties;
    for (let i = 0; i < allProperties.length; ++i) {
      const property = allProperties[i];
      if (!property.range) {
        continue;
      }
      testRunner.log('[\'' + property.name + '\':\'' + property.value + '\'' +
                     (property.important ? ' !important' : '') +
                     (('parsedOk' in property) ? ' non-parsed' : '') + '] @' +
                     cssHelper.rangeText(property.range));
    }
  }

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#mainBody');
  const {result: response} = await dp.CSS.getMatchedStylesForNode({nodeId});

  for (const rule of response.matchedCSSRules) {
    dumpStyleData(rule.rule);
  }
  dumpStyleData(response.inlineStyle);
  testRunner.completeTest();
});
