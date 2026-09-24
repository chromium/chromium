// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      .grid {
        position: absolute;
	top: 44px;
	left: 8px;
        grid-template-columns: 20px 50px;
        grid-template-rows: 100px;
        width: 100px;
        display: grid;
      }
      .ltr-dir {
        direction: ltr;
      }
      .rtl-dir {
        direction: rtl;
      }
      .with-gap {
        grid-gap: 1em;
      }
      </style>

      <p id="description">This test verifies that grids with direction rtl and ltr are correctly highlighted.</p>
      <div>
          <div class="grid ltr-dir" id="ltrGrid">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
          <div class="grid rtl-dir" id="rtlGrid">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
          <div class="grid ltr-dir with-gap" id="ltrGridGap">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
          <div class="grid rtl-dir with-gap" id="rtlGridGap">
              <div style="background: burlywood"></div>
              <div style="background: cadetblue"></div>
          </div>
      </div>
`,
      `This test verifies that grids with direction rtl and ltr are correctly highlighted.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();

  function dumpGridHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumpGridHighlight('ltrGrid');
  await dumpGridHighlight('rtlGrid');
  await dumpGridHighlight('ltrGridGap');
  await dumpGridHighlight('rtlGridGap');

  testRunner.completeTest();
});
