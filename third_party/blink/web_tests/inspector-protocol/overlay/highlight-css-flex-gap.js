// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
      .container {
        position: absolute;
        top: 100px;
        left: 100px;
        width: 400px;
        height: 400px;
        display: flex;
        flex-wrap: wrap;
        flex-direction: row;
        place-content: flex-start;
        column-gap: 10px;
        row-gap: 20px;
      }
      .item {
        width: 100px;
        height: 100px;
      }
    </style>
    <div class="container" id="test-1" style="flex-direction:row;">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-2" style="flex-direction:row-reverse;">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-3" style="flex-direction:column;">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    <div class="container" id="test-4" style="flex-direction:column-reverse;">
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
      <div class="item"></div>
    </div>
    `,
      `This test verifies the gap information sent to the overlay frontend for flex contains with gaps.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumFlexHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumFlexHighlight('test-1');
  await dumFlexHighlight('test-2');
  await dumFlexHighlight('test-3');
  await dumFlexHighlight('test-4');

  testRunner.completeTest();
});
