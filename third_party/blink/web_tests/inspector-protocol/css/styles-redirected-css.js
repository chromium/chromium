// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <div id="test_div">test</div>
    `,
      'Tests that styles in redirected css are editable.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.Fetch.enable({patterns: [{urlPattern: '*dummy*.css*'}]});

  dp.Fetch.onceRequestPaused().then(({params}) => {
    dp.Fetch.fulfillRequest({
      requestId: params.requestId,
      responseCode: 302,
      responsePhrase: 'Found',
      responseHeaders: [
        {name: 'Location', value: 'http://127.0.0.1:8000/dummy-redirected.css'}
      ],
    });
    dp.Fetch.onceRequestPaused().then(({params: p2}) => {
      dp.Fetch.fulfillRequest({
        requestId: p2.requestId,
        responseCode: 200,
        responsePhrase: 'OK',
        responseHeaders: [{name: 'Content-Type', value: 'text/css'}],
        body: btoa('div {\n    background-color: red;\n}\n'),
      });
    });
  });

  const addedPromise = dp.CSS.onceStyleSheetAdded();
  await session.evaluateAsync(() => {
    return new Promise(resolve => {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = 'http://127.0.0.1:8000/dummy.css';
      link.onload = resolve;
      link.onerror = resolve;
      document.head.appendChild(link);
    });
  });
  await addedPromise;

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#test_div');
  const {result} = await dp.CSS.getMatchedStylesForNode({nodeId});
  const match = result.matchedCSSRules.find(m => m.rule.origin === 'regular');
  const prop = match.rule.style.cssProperties.find(
      p => p.name === 'background-color' && p.range);
  const editRes = await dp.CSS.setStyleTexts({
    edits: [{
      styleSheetId: match.rule.style.styleSheetId,
      range: match.rule.style.range,
      text: `/* ${prop.name}: ${prop.value}; */`,
    }],
  });
  if (editRes.error) {
    testRunner.log('FAIL: Could not disable style in redirected css.');
  }
  testRunner.completeTest();
});
