// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
      div {
          color: red;
          background-color: blue;
          font-size: 20px;
      }
      section#section-with-wide-gamut-colors {
        color: color(xyz-d50 2 0.9 0.79);
        background-color: lab(86 26 -3.8);
      }
      </style>
      <div id="empty-div"></div>
      <div id="div-with-text">I have text</div>
      <section id="section-with-wide-gamut-colors">I'm a text</section>
    `,
      `This test verifies the style info overlaid on an inspected node.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpHighlightStyle('empty-div');
  await helper.dumpHighlightStyle('div-with-text');
  await helper.dumpHighlightStyle('section-with-wide-gamut-colors');
  testRunner.completeTest();
});
