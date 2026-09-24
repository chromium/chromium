// Copyright 2017 The Chromium Authors
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
      .outer {
          transform: rotate(-90deg) translate(-200px, -900px);
      }
      .grid {
          width: 1000px;
          display: grid;
          grid-template-columns: 50px 20%;
          grid-auto-rows: 25px;
      }
      .padded {
          padding: 50px 60px;
          align-content: space-around;
          justify-content: end;
          grid-gap: 15px 25px;
          border: 5px solid;
          margin: 10px;
      }
      .parent {
          width: 1300px;
          display: grid;
          grid-template-columns: 50px 1200px;
          grid-auto-rows: 100px;
          grid-gap: 20px 50px;
      }
      .fixed {
          position: absolute;
          top: 0;
          left: 0;
      }
      </style>
      <div class="outer">
        <div id="paddedGrid" class="grid padded fixed">
            <div>c1</div>
            <div>c2</div>
            <div>c3</div>
        </div>
        <div id="parentGrid" class="parent fixed">
            <div>Parent Cell 1</div>
            <div>Parent Cell 2</div>
            <div>Parent Cell 3</div>
            <div id="nestedGrid" class="grid">
                <div>c1</div>
                <div>c2</div>
                <div>c3</div>
            </div>
        </div>
      </div>

      <p id="description">This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS grid div.</p>
    `,
      `This test verifies the position and size of the highlight rectangles overlayed on an inspected CSS grid div.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  function dumpGridHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumpGridHighlight('paddedGrid');
  await dumpGridHighlight('nestedGrid');

  testRunner.completeTest();
});
