// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Debugger.scriptParsed reports valid start and end offsets for inline scripts.');

  const targetUrl = testRunner.url('resources/debugger-scripts.html');
  const scripts = [];
  const {promise: fourScriptsParsed, resolve} = Promise.withResolvers();
  dp.Debugger.onScriptParsed(event => {
    if (event.params.url !== targetUrl) {
      return;
    }
    scripts.push(event.params);
    if (scripts.length === 4) {
      resolve();
    }
  });

  await dp.Debugger.enable();
  await Promise.all([
    session.navigate(targetUrl),
    fourScriptsParsed,
  ]);

  scripts.sort((a, b) =>
                   a.startLine - b.startLine || a.startColumn - b.startColumn);
  for (let i = 0; i < scripts.length; ++i) {
    testRunner.log(`script ${i + 1}:`);
    testRunner.log(
        `    start: ${scripts[i].startLine}:${scripts[i].startColumn}`);
    testRunner.log(`    end: ${scripts[i].endLine}:${scripts[i].endColumn}`);
  }
  testRunner.completeTest();
});
