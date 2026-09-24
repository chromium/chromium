// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
    .container {
      display: flex;
      justify-content: center;
      width: 100px;
      height: 100px;
      gap: 10px;
      align-items: flex-start;
      background: gold;
    }
    .item { padding: 10px; }
    </style>
    <div class="container" id="test-1" style="flex-direction:row-reverse;">
      <div class="item"></div>
      <div class="item" style="align-self: flex-end;"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-2" style="flex-direction:column-reverse;">
      <div class="item"></div>
      <div class="item" style="align-self: flex-end;"></div>
      <div class="item"></div>
    </div>
  `,
      `This test verifies that the flex overlay creates the right lines and items for reverse direction flex containers. See crbug.com/1153272.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumFlexHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumFlexHighlight('test-1');
  await dumFlexHighlight('test-2');

  testRunner.completeTest();
});
