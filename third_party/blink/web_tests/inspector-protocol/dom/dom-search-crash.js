// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
      <iframe src="${
          testRunner.url(
              'resources/dom-search-crash-iframe.html')}" onload="window.iframeLoaded = true"></iframe>
    `,
      'Tests that elements panel search is not crashing on documentElement-less cases.');

  await dp.DOM.enable();

  testRunner.runTestSuite([
    async function testSetUp() {
      await session.evaluateAsync(
          'window.iframeLoaded || new Promise(r => document.querySelector("iframe").addEventListener("load", r))');
      await dp.DOM.getDocument();
    },

    async function testNoCrash() {
      await dp.DOM.performSearch(
          {query: 'FooBar', includeUserAgentShadowDOM: false});
    }
  ]);
})
