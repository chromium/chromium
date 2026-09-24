// Copyright 2017 The Chromium Authors
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
        #grid-with-areas {
            position: absolute;
            top: 0;
            left: 0;
            width: 401px;
            height: 601px;
            display: grid;
            grid-gap: 10px;
            grid-template-columns: 1fr 1fr;
            grid-template-rows: 1fr 1fr;
            grid-template-areas:
            "header  header"
            "sidebar main";
        }
        #grid-with-areas .header { grid-area: header; }
        #grid-with-areas .sidebar { grid-area: sidebar; }
        #grid-with-areas .main { grid-area: main; }
        </style>
        <div id="grid-with-areas">
            <div class="header">header</div>
            <div class="sidebar">sidebar</div>
            <div class="main">main</div>
        </div>

        <p id="description">This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid areas.</p>
      `,
      `This test verifies the names, positions and sizes of the highlight rectangles overlayed on CSS Grid areas.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpStableHighlight('grid-with-areas');

  testRunner.completeTest();
});
