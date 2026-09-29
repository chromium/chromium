// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <div id="container" style="display:none">
      <p>WebKit is used by <a href="http://www.apple.com/safari/">Safari</a>, Dashboard, etc..</p>
      <h2>Getting involved</h2>
      <p id="identity">There are many ways to get involved. You can:</p>
      <ul>
         <li></li>
      </ul>
      <ul>
         <li></li>
      </ul>
      </div>
    `,
      'Tests undo for the DOMAgent.setOuterHTML protocol method (part 2).');
  const SetOuterHTMLTest =
      await testRunner.loadScript('resources/set-outer-html-test.js');
  const setOuterHTMLTest = new SetOuterHTMLTest(testRunner, session, dp);
  await session.evaluate(`
      document.getElementById("identity").wrapperIdentity = "identity";
  `);

  testRunner.runTestSuite([
    async function testSetUp() {
      await setOuterHTMLTest.setUp();
    },

    async function testChangeMultipleThings() {
      var text = setOuterHTMLTest.containerText.replace(/<li>.*<\/li>/, '');
      text = text.replace('<h2>', '<h2 foo="bar" bar="baz">');
      await setOuterHTMLTest.setOuterHTMLUseUndo(text);
    },

    async function testChangeNestingLevel() {
      var text = setOuterHTMLTest.containerText.replace('<ul>', '<div><ul>');
      var text = text.replace('</ul>', '</ul></div>');
      await setOuterHTMLTest.setOuterHTMLUseUndo(text);
    },

    async function testSwapNodes() {
      var text = setOuterHTMLTest.containerText.replace(
          '<h2>Getting involved</h2>', '');
      var text = text.replace('</div>', '<h2>Getting involved</h2></div>');
      await setOuterHTMLTest.setOuterHTMLUseUndo(text);
    },

    async function testEditTwoRoots() {
      var text = setOuterHTMLTest.containerText + '<div>Additional node</div>';
      await setOuterHTMLTest.setOuterHTMLUseUndo(text);
    },

    async function testDupeNode() {
      await setOuterHTMLTest.patchOuterHTML(
          '<h2>Getting involved</h2>',
          '<h2>Getting involved</h2><h2>Getting involved</h2>');
    }
  ]);
})
