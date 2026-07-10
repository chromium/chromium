// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
      <div id="target" style="display: list-item;">
        <style>
          #target::before { content: "BEFORE"; }
          #target::after { content: "AFTER"; }
        </style>
        Content
      </div>
  `, 'Tests that DOM.setInspectedNode works for before, after, and marker pseudo elements.');

  const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
  const targetResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#target'});
  const targetNodeId = targetResponse.result.nodeId;

  const targetDescription = await dp.DOM.describeNode({nodeId: targetNodeId, depth: 1});
  const pseudoElements = targetDescription.result.node.pseudoElements;

  if (!pseudoElements || pseudoElements.length === 0) {
    testRunner.log("FAIL: No pseudo elements found on target");
    testRunner.completeTest();
    return;
  }

  // Sort them to have deterministic output order
  pseudoElements.sort((a, b) => a.nodeName.localeCompare(b.nodeName));

  for (const pseudo of pseudoElements) {
    const pseudoName = pseudo.nodeName;
    if (pseudoName === '::before' || pseudoName === '::after' || pseudoName === '::marker') {
      await dp.DOM.setInspectedNode({nodeId: pseudo.nodeId});
      const evaluateResponse = await dp.Runtime.evaluate({
        expression: `({
          isCSSPseudoElement: $0 instanceof CSSPseudoElement,
          type: $0?.type,
          originatingTagName: $0?.element?.tagName
        })`,
        includeCommandLineAPI: true,
        returnByValue: true
      });
      testRunner.log(`Inspected ${pseudoName}: ${JSON.stringify(evaluateResponse.result.result.value)}`);
    }
  }

  testRunner.completeTest();
})
