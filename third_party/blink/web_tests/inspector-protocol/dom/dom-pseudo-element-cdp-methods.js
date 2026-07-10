// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
      <div id="target" style="display: list-item; width: 100px; height: 100px;">
        <style>
          #target::before { content: "BEFORE"; display: block; width: 50px; height: 50px; }
          #target::after { content: "AFTER"; display: block; width: 50px; height: 50px; }
        </style>
        Content
      </div>
  `, 'Tests that DOM.resolveNode and objectId-based CDP commands work correctly with CSSPseudoElement.');

  await dp.DOM.enable();

  const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
  const targetResponse = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#target'});
  const targetNodeId = targetResponse.result.nodeId;

  const targetDescription = await dp.DOM.describeNode({nodeId: targetNodeId, depth: 1});
  const pseudoElements = targetDescription.result.node.pseudoElements;

  const beforePseudo = pseudoElements ? pseudoElements.find(p => p.nodeName === '::before') : null;
  if (!beforePseudo) {
    testRunner.log('FAIL: ::before pseudo element not found');
    testRunner.completeTest();
    return;
  }

  // 1. Test DOM.resolveNode for pseudo element
  const resolveResponse = await dp.DOM.resolveNode({nodeId: beforePseudo.nodeId});
  const remoteObject = resolveResponse.result.object;
  testRunner.log(`DOM.resolveNode className: ${remoteObject.className}`);

  // 2. Test DOM.setInspectedNode and compare with DOM.resolveNode object
  await dp.DOM.setInspectedNode({nodeId: beforePseudo.nodeId});
  const evalResponse = await dp.Runtime.evaluate({
    expression: `$0 instanceof CSSPseudoElement && $0.type === '::before'`,
    includeCommandLineAPI: true,
    returnByValue: true
  });
  testRunner.log(`$0 is CSSPseudoElement ::before: ${evalResponse.result.result.value}`);

  // 3. Test DOM.requestNode with objectId of resolved pseudo element
  const requestNodeResponse = await dp.DOM.requestNode({objectId: remoteObject.objectId});
  testRunner.log(`DOM.requestNode nodeId matches: ${requestNodeResponse.result.nodeId === beforePseudo.nodeId}`);

  // 4. Test DOM.getBoxModel with objectId of resolved pseudo element
  const boxModelResponse = await dp.DOM.getBoxModel({objectId: remoteObject.objectId});
  testRunner.log(`DOM.getBoxModel success: ${Boolean(boxModelResponse.result?.model)}`);

  testRunner.completeTest();
})
