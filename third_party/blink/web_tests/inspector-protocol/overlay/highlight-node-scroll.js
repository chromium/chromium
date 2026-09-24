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
      .inspected {
          margin: 5px;
          border: solid 10px aqua;
          padding: 15px;
          width: 200px;
          height: 200px;
          background-color: blue;
          float: left;
      }
      #scrollingContainer {
          clear: both;
          width: 100px;
          height: 100px;
          overflow: auto;
      }
      #description {
          clear: both;
      }

      </style>
      <div id="inspectedElement1" class="inspected"></div>

      <div id="scrollingContainer">
          <div id="inspectedElement2" class="inspected"></div>
      </div>

      <p id="description"></p>
    `,
      `This test verifies the position and size of the highlight rectangles overlayed on an inspected div in the scrolled view.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();

  await session.evaluateAsync(`
      window.scrollBy(50, 100);
      scrollingContainer = document.getElementById("scrollingContainer");
      scrollingContainer.scrollTop = 50;
      scrollingContainer.scrollLeft = 60;
  `);

  await helper.dumpHighlight('inspectedElement1');
  await helper.dumpHighlight('inspectedElement2');
  testRunner.completeTest();
});
