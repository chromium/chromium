// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      .grid-lanes {
        display: grid-lanes;
        position: absolute;
        top: 50px;
        left: 10px;
        grid-template-columns: 20px 50px;
        width: 100px;
        height: 100px;
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

      <div>
          <div class="grid-lanes ltr-dir" id="ltrGridLanes">
              <div style="width: 100%; height: 100px; background: burlywood"></div>
              <div style="width: 100%; height: 100px; background: cadetblue"></div>
          </div>
          <div class="grid-lanes rtl-dir" id="rtlGridLanes">
              <div style="width: 100%; height: 100px; background: burlywood"></div>
              <div style="width: 100%; height: 100px; background: cadetblue"></div>
          </div>
          <div class="grid-lanes ltr-dir with-gap" id="ltrGridLanesGap">
              <div style="width: 100%; height: 100px; background: burlywood"></div>
              <div style="width: 100%; height: 100px; background: cadetblue"></div>
          </div>
          <div class="grid-lanes rtl-dir with-gap" id="rtlGridLanesGap">
              <div style="width: 100%; height: 100px; background: burlywood"></div>
              <div style="width: 100%; height: 100px; background: cadetblue"></div>
      </div>
`,
      `This test verifies that grid-lanes layouts with direction rtl and ltr are correctly highlighted.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();

  function dumpGridHighlight(id) {
    return helper.dumpHighlight(id);
  }

  await dumpGridHighlight('ltrGridLanes');
  await dumpGridHighlight('rtlGridLanes');
  await dumpGridHighlight('ltrGridLanesGap');
  await dumpGridHighlight('rtlGridLanesGap');

  testRunner.completeTest();
});
