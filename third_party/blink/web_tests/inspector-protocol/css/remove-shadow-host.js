// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
        <!DOCTYPE html>
        <div id="host"></div>
      `,
                                                   'Test remove shadow host.');

  const styleSheetHeaders = new Map();
  await dp.DOM.enable();
  await dp.CSS.enable();

  await session.evaluate(`
        function addShadow() {
          var root = host.attachShadow({mode:"open"});
          root.innerHTML = '<link rel="stylesheet" href="data:text/css,#x{color:pink}">';
        }
    `);

  dp.CSS.onStyleSheetAdded(({params: {header}}) => {
    styleSheetHeaders.set(header.styleSheetId, header);
    testRunner.log('Sheet added: ' + header.sourceURL);
    session.evaluate('host.remove()');
  });

  dp.CSS.onStyleSheetRemoved(({params: {styleSheetId}}) => {
    const header = styleSheetHeaders.get(styleSheetId);
    testRunner.log('Sheet removed: ' + (header ? header.sourceURL : ''));
    testRunner.completeTest();
  });

  await session.evaluate('addShadow()');
});
