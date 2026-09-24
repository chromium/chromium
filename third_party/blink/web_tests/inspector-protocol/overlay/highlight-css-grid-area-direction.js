// Copyright 2026 The Chromium Authors
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
      .grid {
        position: absolute;
        top: 0;
        left: 0;
        width: 400px;
        height: 300px;
        display: grid;
        grid-template-columns: 100px 200px;
        grid-template-rows: 150px 150px;
        grid-template-areas:
          "a b"
          "c d";
      }
      .with-gap {
        grid-gap: 20px 10px;
        grid-template-columns: 100px 100px 100px;
        grid-template-rows: 100px 100px;
        grid-template-areas:
          "header  header  header"
          "sidebar main    main";
      }
      .ltr-dir {
        direction: ltr;
      }
      .rtl-dir {
        direction: rtl;
      }
      </style>

      <div class="grid ltr-dir" id="ltrGrid">
        <div style="grid-area: a">a</div>
        <div style="grid-area: b">b</div>
        <div style="grid-area: c">c</div>
        <div style="grid-area: d">d</div>
      </div>
      <div class="grid rtl-dir" id="rtlGrid">
        <div style="grid-area: a">a</div>
        <div style="grid-area: b">b</div>
        <div style="grid-area: c">c</div>
        <div style="grid-area: d">d</div>
      </div>
      <div class="grid ltr-dir with-gap" id="ltrGridGap">
        <div style="grid-area: header">header</div>
        <div style="grid-area: sidebar">sidebar</div>
        <div style="grid-area: main">main</div>
      </div>
      <div class="grid rtl-dir with-gap" id="rtlGridGap">
        <div style="grid-area: header">header</div>
        <div style="grid-area: sidebar">sidebar</div>
        <div style="grid-area: main">main</div>
      </div>

      <p id="description">This test verifies that grid areas with direction rtl and ltr are correctly highlighted.</p>
    `,
      `This test verifies that grid areas with direction rtl and ltr are correctly highlighted.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpStableHighlight('ltrGrid');
  await helper.dumpStableHighlight('rtlGrid');
  await helper.dumpStableHighlight('ltrGridGap');
  await helper.dumpStableHighlight('rtlGridGap');

  testRunner.completeTest();
});
