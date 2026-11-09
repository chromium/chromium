// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <div id="rootElement" style="color: red">
      <div id="elementToRemove"></div>
      </div>
    `,
      'Tests that DOMDebugger does not crash or pause on DOM breakpoints when editing DOM nodes via DOM.removeNode, DOM.setAttributesAsText, and DOM.setAttributeValue. Chromium bug 249655');

  await dp.DOM.enable();
  await dp.Debugger.enable();
  dp.Debugger.onPaused(() => {
    testRunner.log('FAIL: paused');
    dp.Debugger.resume();
  });
  const {result: {root}} = await dp.DOM.getDocument();

  function logResponse(label, response) {
    testRunner.log(
        `${label}: ${response.error ? response.error.message : 'OK'}`);
  }

  testRunner.runTestSuite([
    async function testRemoveNode() {
      testRunner.log('Testing NodeRemoved DOM breakpoint.');
      const {result: {nodeId}} = await dp.DOM.querySelector(
          {nodeId: root.nodeId, selector: '#elementToRemove'});
      logResponse('DOMDebugger.setDOMBreakpoint',
                  await dp.DOMDebugger.setDOMBreakpoint(
                      {nodeId, type: 'node-removed'}));
      logResponse('DOM.removeNode', await dp.DOM.removeNode({nodeId}));
    },

    async function testModifyAttribute() {
      testRunner.log('Testing AttributeModified DOM breakpoint.');
      const {result: {nodeId}} = await dp.DOM.querySelector(
          {nodeId: root.nodeId, selector: '#rootElement'});
      logResponse('DOMDebugger.setDOMBreakpoint',
                  await dp.DOMDebugger.setDOMBreakpoint(
                      {nodeId, type: 'attribute-modified'}));
      logResponse('DOM.setAttributesAsText',
                  await dp.DOM.setAttributesAsText(
                      {nodeId, text: 'a title', name: 'title'}));
      logResponse('DOM.setAttributeValue',
                  await dp.DOM.setAttributeValue(
                      {nodeId, name: 'title', value: 'a title'}));
    }
  ]);
});
