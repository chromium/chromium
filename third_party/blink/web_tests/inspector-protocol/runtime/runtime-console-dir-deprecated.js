// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<div id="foo"></div>`,
      'Tests that console.dir does not report deprecation issues while generating object previews.');

  await dp.Runtime.enable();
  await dp.Audits.enable();

  dp.Runtime.onConsoleAPICalled(({params}) => {
    const arg = params.args[0];
    const frame = params.stackTrace.callFrames[0];
    const hasPreview = (arg.preview?.properties.length ?? 0) > 0;
    testRunner.log(`${frame.url}:${frame.lineNumber + 1} ${
        arg.description ?? arg.type} (hasPreview=${hasPreview})`);
  });
  dp.Audits.onIssueAdded(({params}) => {
    testRunner.log(`${params.issue.code}: ${
        params.issue.details?.deprecationIssueDetails?.type}`);
  });

  await session.evaluate(`
    function logObjects() {
      console.dir(window);
      console.dir((document.createRange()).expand('document'));
    }
    //# sourceURL=runtime-console-dir-deprecated.js
  `);

  const issuePromise = dp.Audits.onceIssueAdded();
  await session.evaluate('logObjects()');
  await issuePromise;

  testRunner.completeTest();
});
