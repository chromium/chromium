// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <iframe src="${
          testRunner.url(
              'resources/set-outer-html-body-iframe.html')}" onload="window.iframeLoaded = true"></iframe>
    `,
      'Tests DOMAgent.setOuterHTML invoked on body tag. See https://bugs.webkit.org/show_bug.cgi?id=62272. ');

  var htmlNodeId;
  var bodyNodeId;
  var headNodeId;

  testRunner.runTestSuite([
    async function testSetUp() {
      await session.evaluateAsync(
          'window.iframeLoaded || new Promise(r => document.querySelector("iframe").addEventListener("load", r))');
      await dp.DOM.enable();
      const {result: {root}} =
          await dp.DOM.getDocument({depth: -1, pierce: true});
      function findById(node, id) {
        if (node.attributes) {
          for (let i = 0; i < node.attributes.length; i += 2) {
            if (node.attributes[i] === 'id' && node.attributes[i + 1] === id)
              return node.nodeId;
          }
        }
        if (node.contentDocument) {
          const found = findById(node.contentDocument, id);
          if (found)
            return found;
        }
        for (const child of node.children || []) {
          const found = findById(child, id);
          if (found)
            return found;
        }
        return 0;
      }
      htmlNodeId = findById(root, 'html');
      headNodeId = findById(root, 'head');
      bodyNodeId = findById(root, 'body');
    },

    async function testSetBody() {
      await dp.DOM.setOuterHTML(
          {nodeId: bodyNodeId, outerHTML: '<body>New body content</body>'});
      await dumpHTML();
    },

    async function testInsertComments() {
      await dp.DOM.setOuterHTML({
        nodeId: bodyNodeId,
        outerHTML:
            '<!-- new comment between head and body --><body>New body content</body>'
      });
      await dumpHTML();
    },

    async function testSetHead() {
      await dp.DOM.setOuterHTML({
        nodeId: headNodeId,
        outerHTML: '<head><!-- new head content --></head>'
      });
      await dumpHTML();
    },

    async function testSetHTML() {
      await dp.DOM.setOuterHTML({
        nodeId: htmlNodeId,
        outerHTML:
            '<html><head><!-- new head content --></head><body>Setting body as a part of HTML.</body></html>'
      });
      await dumpHTML();
    }
  ]);

  async function dumpHTML() {
    var text =
        (await dp.DOM.getOuterHTML({nodeId: htmlNodeId})).result.outerHTML;
    testRunner.log(text);
  }
})
