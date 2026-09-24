// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
      body {
          width: 2000px;
          height: 2000px;
          background-color: grey;
      }
      .grid-lanes-columns {
          display: grid-lanes;
          grid-template-columns: 50px 20% 30%;
          width: 500px;
          gap: 10px;
      }
      .grid-lanes-rows {
          display: grid-lanes;
          grid-template-rows: 100px repeat(2, 30px 10%);
          grid-lanes-direction: row;
          height: 500px;
          width: 300px;
          gap: 20px;
      }
      .empty-grid-lanes {
          display: grid-lanes;
          width: 200px;
          height: 200px;
          grid-template-columns: 10% 20%;
      }
    </style>
    <div id="grid-lanes-columns-container" class="grid-lanes-columns">
        <div style="width: 100%; height: 50px;"></div>
        <div style="width: 100%; height: 30px;"></div>
        <div style="width: 100%; height: 25px;"></div>
        <div style="width: 100%; height: 100px;"></div>
        <div style="width: 100%; height: 80px;"></div>
        <div style="width: 100%; height: 60px;"></div>
    </div>
    <div id="grid-lanes-rows-container" class="grid-lanes-rows">
        <div style="width:50px; height: 100%;"></div>
        <div style="width:100px; height: 100%;"></div>
        <div style="width:150px; height: 100%;"></div>
        <div style="width:200px; height: 100%;"></div>
        <div style="width:250px; height: 100%;"></div>
        <div style="width:30px; height: 100%;"></div>
    </div>
    <div id="empty-grid-lanes-container" class="empty-grid-lanes"></div>
    <p id="description">This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS grid-lanes div.</p>
    `,
      `This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS grid-lanes div.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumpGridLanesHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumpGridLanesHighlight('grid-lanes-columns-container');
  await dumpGridLanesHighlight('grid-lanes-rows-container');
  await dumpGridLanesHighlight('empty-grid-lanes-container');

  testRunner.completeTest();
});
