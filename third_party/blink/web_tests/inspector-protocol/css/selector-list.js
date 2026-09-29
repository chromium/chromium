// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
      `
      <style>
      #inspected{
      }

      #InSpEcTeD {
      }

      /* */#inspected/* */ {
      }

      /*
       */ FOO/*Single-line 1*/ bAr,/* Single-line 2*/#inspected/*
          Multiline comment
      */ ,MOO>BAR, /*1*/htML /*2
      */div/*3*/,/**/Foo~/**C*/Moo,/**/MOO /* Comment
       */
      {
        color: green;
      }

      </style>

      <div id="inspected">Text</div>
    `,
      'Tests representation of selector lists in the protocol. Bug 103118.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
  await cssHelper.dumpSelectedNodeStyles(nodeId);
  testRunner.completeTest();
});
