// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
    body {
        width: 1000px;
        height: 1000px;
    }
    #grid-with-line-names {
        position: absolute;
        top: 0;
        left: 0;
        display: grid;
        gap: 10px;
        grid-template-columns: [fullpage-start] 100px [content-start header] 500px [content-end] 20px [fullpage-end];
        grid-template-rows: [header] 200px [main article images] repeat(3, [section] 200px) [end];
    }
    </style>
    <div id="grid-with-line-names"></div>

    <p id="description">This test verifies the names and positions of named grid lines are generated correctly.</p>
  `,
      `This test verifies the names and positions of named grid lines are generated correctly.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpStableHighlight('grid-with-line-names');
  testRunner.completeTest();
});
