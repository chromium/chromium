// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
    #flex-container {
      position: absolute;
      top: 100px;
      left: 100px;
      width: 400px;
      height: 400px;
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      align-content: space-between;
      justify-content: space-between;
    }
    .item {
      width: 100px;
      height: 100px;
    }
    </style>
    <div id="flex-container">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    `,
      `This test verifies the position and size of the highlighted lines and items in a multiline flex container.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumFlexHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumFlexHighlight('flex-container');

  testRunner.completeTest();
});
