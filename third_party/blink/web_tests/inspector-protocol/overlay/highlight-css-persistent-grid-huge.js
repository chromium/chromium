// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      #big-grid {
          display: grid;
          grid-template-columns: repeat(100, 5px);
          grid-template-rows: repeat(100, 5px);
          grid-gap: 5px;
      }
      #huge-grid {
          display: grid;
          grid-template-columns: repeat(10000, 5px);
          grid-template-rows: repeat(10000, 5px);
          grid-gap: 5px;
      }
      </style>
      <div id="big-grid"></div>
      <div id="huge-grid"></div>

      <p id="description">This test verifies that huge CSS grids can be highlighted.</p>
    `,
      `This test verifies that huge CSS grids can be highlighted with the grid Highlight Tool.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  // Dump the highlight of a 100x100 grid to check that it's not absurdly huge.
  await helper.dumpGridHighlights(['big-grid']);


  // Now check that the highlight data for a 10000x10000 grid can be generated.
  // But don't dump it since that would make the test time out.
  const node = await helper.nodeWithId('huge-grid');
  (await dp.Overlay.getGridHighlightObjectsForTest({
    nodeIds: [node.id]
  })).result;

  testRunner.completeTest();
});
