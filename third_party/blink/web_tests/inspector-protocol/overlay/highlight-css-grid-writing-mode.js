// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      .grid {
        position: absolute;
        top: 50px;
        left: 50px;
        display: grid;
        grid-template-rows: 20px 50px;
        grid-template-columns: 100px 200px;
        gap: 10px;
      }
      #verticalRl {
        writing-mode: vertical-rl;
      }
      #verticalLr {
        writing-mode: vertical-lr;
      }
      #sidewaysRl {
        writing-mode: sideways-rl;
      }
      #sidewaysLr {
        writing-mode: sideways-lr;
      }
      </style>

      <p id="description">This test verifies that similarly-sized grids with different writing-modes share the same grid information but have a different writingMode value.</p>
      <div>
          <div class="grid" id="horizontalTb">
              <div style="background: burlywood">1</div>
              <div style="background: cadetblue">2</div>
              <div style="background: aquamarine">3</div>
              <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="verticalRl">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="verticalLr">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="sidewaysRl">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
          <div class="grid" id="sidewaysLr">
            <div style="background: burlywood">1</div>
            <div style="background: cadetblue">2</div>
            <div style="background: aquamarine">3</div>
            <div style="background: peachpuff">4</div>
          </div>
      </div>
  `,
      `This test verifies that similarly-sized grids with different writing-modes share the same grid information but have a different writingMode value.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function getWritingMode(highlightObject) {
    return highlightObject.gridInfo[0].writingMode;
  }

  function getWritingModeRoot(highlightObject) {
    return highlightObject.gridInfo[0].writingModeRoot;
  }

  function getGridInfo(highlightObject) {
    const info = highlightObject.gridInfo[0];
    // Drop writing-mode specific metadata and compare the remaining grid
    // geometry.
    return JSON.stringify(
        info,
        (key, value) => (key === 'writingMode' || key === 'writingModeRoot') ?
            undefined :
            value,
        2);
  }

  const horizontalTbNode = await helper.nodeWithId('horizontalTb');
  const {highlight: horizontalTbHighlight} =
      (await dp.Overlay.getHighlightObjectForTest({
        nodeId: horizontalTbNode.id
      })).result;
  const horizontalTbInfo = getGridInfo(horizontalTbHighlight);

  testRunner.log(`Node id #horizontalTb writing-mode: ${
      getWritingMode(horizontalTbHighlight)}`);
  if (getWritingModeRoot(horizontalTbHighlight))
    testRunner.log('FAIL: ' +
                   '#horizontalTb writingModeRoot presence');
  testRunner.log(`Grid info: ${horizontalTbInfo}`);

  for (const id of ['verticalRl', 'verticalLr', 'sidewaysRl', 'sidewaysLr']) {
    const node = await helper.nodeWithId(id);
    const {highlight: result} =
        (await dp.Overlay.getHighlightObjectForTest({nodeId: node.id})).result;
    const gridInfo = getGridInfo(result);
    const writingModeRoot = getWritingModeRoot(result);

    testRunner.log(`Node id #${id} writing-mode: ${getWritingMode(result)}`);
    if (id === 'verticalLr' !== Boolean(writingModeRoot))
      testRunner.log('FAIL: ' +
                     `#${id} writingModeRoot presence`);
    if (writingModeRoot) {
      if (!Number.isFinite(writingModeRoot.x))
        testRunner.log('FAIL: ' +
                       `#${id} writingModeRoot.x`);
      if (!Number.isFinite(writingModeRoot.y))
        testRunner.log('FAIL: ' +
                       `#${id} writingModeRoot.y`);
    }
    testRunner.log(`Grid info: ${gridInfo}`);
    testRunner.log(`Should be the same as #horizontalTb: ${
        gridInfo === horizontalTbInfo}`);
  }

  testRunner.completeTest();
});
