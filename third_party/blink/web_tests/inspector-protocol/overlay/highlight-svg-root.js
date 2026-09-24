// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <!DOCTYPE html>
      <style>

      body {
          margin: 0;
      }
      #svg-root {
          margin: 5px;
          border: solid 10px aqua;
          padding: 15px;
          background-color: blue;
      }

      </style>
      <svg id="svg-root" width="100" height="100" viewBox="0 0 50 50"/>
      <p id="description"></p>
    `,
      `This test verifies the position and size of the highlight rectangles overlayed on an SVG root element.\n`);

  const OverlayHelper =
      await testRunner.loadScript('./resources/highlight-test-helper.js');
  const helper = new OverlayHelper(testRunner, dp, session);
  await helper.init();


  await helper.dumpHighlight('svg-root');
  testRunner.completeTest();
});
