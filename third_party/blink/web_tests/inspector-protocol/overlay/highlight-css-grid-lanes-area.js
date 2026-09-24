// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
        <style>
        body {
            width: 1000px;
            height: 1000px;
            background-color: grey;
        }
        #grid-lanes-with-areas {
            width: 400px;
            height: 600px;
            display: grid-lanes;
            grid-template-columns: 2fr 1fr 1fr;
            grid-template-areas:
            "header sidebar main";
            gap: 10px;
        }
        #grid-lanes-with-areas .header { grid-area: header; }
        #grid-lanes-with-areas .sidebar { grid-area: sidebar; }
        #grid-lanes-with-areas .main { grid-area: main; }
        </style>
        <div id="grid-lanes-with-areas">
            <div class="header">header</div>
            <div class="sidebar">sidebar</div>
            <div class="main">main</div>
        </div>

        <p id="description">This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid Lanes areas.</p>
      `,
      `This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid Lanes areas.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpStableHighlight('grid-lanes-with-areas');

  testRunner.completeTest();
});
