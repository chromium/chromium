// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <div id="container" style="display:none">
        <child id="identity"></child>
      </div>
    `,
      'Tests that DOMAgent.setOuterHTML can handle whitespace-only text nodes.');
  const SetOuterHTMLTest =
      await testRunner.loadScript('resources/set-outer-html-test.js');
  const setOuterHTMLTest = new SetOuterHTMLTest(testRunner, session, dp);
  await session.evaluate(`
      document.getElementById("identity").wrapperIdentity = "identity";
  `);

  async function setChildTextContent(textContent) {
    var text = setOuterHTMLTest.containerText.replace(
        /<child id="identity">.*<\/child>/,
        `<child id="identity">${textContent}</child>`);
    testRunner.log(`Setting textContent to "${textContent}"`);
    await dp.DOM.setOuterHTML(
        {nodeId: setOuterHTMLTest.containerId, outerHTML: text});
    dumpEvents();
  }

  function dumpEvents() {
    setOuterHTMLTest.events.sort();

    for (let i = 0; i < setOuterHTMLTest.events.length; ++i)
      testRunner.log(setOuterHTMLTest.events[i]);

    setOuterHTMLTest.events.length = 0;  // 'events' is readonly.
    testRunner.log('');
  }

  await setOuterHTMLTest.setUp();
  await setChildTextContent(' ');
  await setChildTextContent('NOT_WHITESPACE');
  await setChildTextContent('OTHER_NOT_WHITESPACE');
  await setChildTextContent('   ');
  await setChildTextContent('');
  testRunner.completeTest();
})
