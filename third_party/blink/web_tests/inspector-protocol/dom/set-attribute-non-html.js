// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/set-attribute-non-html.svg',
      'Tests that elements panel updates dom tree structure upon setting attribute on non HTML elements. PASSes if there is no crash.');
  var targetNodeId;

  testRunner.runTestSuite([
    async function testDumpInitial() {
      await dp.DOM.enable();
      const {result: {root}} = await dp.DOM.getDocument({depth: -1});
      const {result: {nodeId}} =
          await dp.DOM.querySelector({nodeId: root.nodeId, selector: '#node'});
      targetNodeId = nodeId;
    },

    async function testSetAttributeText() {
      await dp.DOM.setAttributesAsText({
        nodeId: targetNodeId,
        name: 'foo',
        text: 'foo2=\'baz2\' foo3=\'baz3\''
      });
    }
  ]);
})
