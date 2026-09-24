// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      #flex-container {
        width: 500px;
        height: 100px;
        display: flex;
      }
      .item {
        margin: 10px;
        flex: 1;
      }
      button {
        width: 50px;
        height: 50px;
        border: 0;
        padding: 0;
      }
      </style>
      <div id="flex-container">
        <div class="item"></div>
        <div class="item"></div>
        <div class="item"></div>
      </div>
      <button id="should-not-be-flexbox">click</button>
      <p id="description">This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS flex div.</p>
    `,
      `This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS flex div.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumFlexHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumFlexHighlight('flex-container');
  await dumFlexHighlight('should-not-be-flexbox');

  testRunner.completeTest();
});
