// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startHTML(
      `
     <h1 id="node1">accessible name</h1>
    `,
      'Tests that we cannot find elements inside OOPIF by accessible name');
  session.evaluate(`
    const frame = document.createElement('iframe');
    frame.setAttribute('src', 'http://devtools.oopif.test:8000/inspector-protocol/resources/iframe-accessible-name.html');
    document.body.appendChild(frame);
  `);


  const documentResp = await dp.DOM.getDocument();
  const documentId = documentResp.result.root.nodeId;

  const documentResp2 = await dp.DOM.resolveNode({nodeId: documentId});
  const documentObjId = documentResp2.result.object.objectId;

  async function testGetIdsForSubtreeByAccessibleNameOOPIF() {
    const response = await dp.Accessibility.queryAXTree(
        {objectId: documentObjId, accessibleName: 'accessible name'});
    await logNodes(response.result.nodes);
  }

  // copied from third_party/blink/web_tests/inspector-protocol/accessibility/accessibility-query-axtree.js
  async function logNodes(axNodes) {
    for (const axNode of axNodes) {
      const backendNodeId = axNode.backendDOMNodeId;
      const response = await dp.DOM.describeNode({backendNodeId});
      const node = response.result.node;
      // we can only print ids for ELEMENT_NODEs, skip TEXT_NODEs
      if (node.nodeType !== Node.ELEMENT_NODE) {
        continue;
      }
      const nodeAttributes = node.attributes;
      const idIndex = nodeAttributes.indexOf('id') + 1;
      testRunner.log(nodeAttributes[idIndex]);
    }
  }

  testRunner.runTestSuite([testGetIdsForSubtreeByAccessibleNameOOPIF]);
});
