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
      </div>
    `,
      'Tests DOMAgent.setOuterHTML protocol method.');
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

    async function testChangeCharacterData() {
      await setOuterHTMLTest.patchOuterHTML('Getting involved',
                                            'Getting not involved');
    },

    async function testChangeAttributes() {
      await setOuterHTMLTest.patchOuterHTML('<a href', '<a foo="bar" href');
    },

    async function testRemoveLastChild() {
      await setOuterHTMLTest.patchOuterHTML('Getting involved', '');
    },

    async function testSplitNode() {
      await setOuterHTMLTest.patchOuterHTML('Getting involved',
                                            'Getting</h2><h2>involved');
    },

    async function testChangeNodeName() {
      await setOuterHTMLTest.patchOuterHTML('<h2>Getting involved</h2>',
                                            '<h3>Getting involved</h3>');
    }
  ]);
})
