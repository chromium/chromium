// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <style>
      /* color: red */

      @media /* color: red */ not /* color: red */print /* color: red */ {
          /* color: red */
          /* color: red */#main/* color: red */{/* color: red */ background /* color: red */ :/* color: red */ blue /* color: red */;/* color: red */ }
          /* color: red */
      }

      /* color: red */

      @font-face {
        /* color: red */
        /* color: red */font-family/* color: red */:/* color: red */ Example/* color: red */;/* color: red */
        /* color: red */
        /* color: red */src/* color: red */:/* color: red */ url(bogus-example-url)/* color: red */;/* color: red */
        /* color: red */
      }

      /* color: red */

      #main /* color: red */{
        /* color: red */color/* color: red */:/* color: red */ green /* color: red */;/* color: red */
      }
      /* color: red */
      @page /* color: red */:right /* color: red */{/* color: red */
        /* color: red */margin-left/* color: red */:/* color: red */ 3cm/* color: red */;/* color: red */
        /* color: red */margin-right /* color: red */: /* color: red */4cm /* color: red */
      }/*color: red*/

      /* edge cases */
      @import/**/;
      /**/{}
      </style>
      <div id="main"></div>
    `,
      'Tests that comments in stylesheets are parsed correctly by the DevTools.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#main');
  testRunner.log('Main style:');
  await cssHelper.dumpSelectedNodeStyles(nodeId, true);
  testRunner.completeTest();
});
