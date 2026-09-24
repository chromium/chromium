// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      html, body {
        margin: 0;
      }
      .container {
        width: 100px;
        height: 100px;
        display: flex;
      }
      button {
        width: 50px;
        height: 50px;
        border: 0;
        padding: 0;
      }
      </style>
      <div id="flex-start" class="container" style="align-items:flex-start"><div class="item"></div></div>
      <div id="flex-end" class="container" style="align-items:flex-end"><div class="item"></div></div>
      <div id="center" class="container" style="align-items:center"><div class="item"></div></div>
      <div id="stretch" class="container" style="align-items:stretch"><div class="item"></div></div>
      <div id="normal" class="container" style="align-items:normal"><div class="item"></div></div>
      <p id="description">This test verifies the flex self-alignment value sent by the backend.</p>
    `,
      `This test verifies the flex self-alignment value sent by the backend.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumpFlexHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumpFlexHighlight('flex-start');
  await dumpFlexHighlight('flex-end');
  await dumpFlexHighlight('center');
  await dumpFlexHighlight('stretch');
  await dumpFlexHighlight('normal');

  testRunner.completeTest();
});
