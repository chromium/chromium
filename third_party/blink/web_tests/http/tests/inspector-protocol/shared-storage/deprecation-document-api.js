// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that deprecation issues are reported for shared storage.\n`);

  await dp.Audits.enable();

  async function evaluateAndLogIssue(js) {
    // Unmute deprecation issue reporting by navigating.
    await page.navigate(
        'https://a.test:8443/inspector-protocol/resources/empty.html');
    let promise = dp.Audits.onceIssueAdded();
    session.evaluateAsync(js).catch(e => testRunner.log(e));
    let result = await promise;

    // The frameId and scriptId are dynamic, so we remove them from the output.
    delete result.params.issue.details.deprecationIssueDetails.affectedFrame
        .frameId;
    delete result.params.issue.details.deprecationIssueDetails
        .sourceCodeLocation.scriptId;
    testRunner.log(result.params, 'Inspector issue: ');
  }

  await evaluateAndLogIssue(`window.sharedStorage.set('key', 'value')`);
  await evaluateAndLogIssue(`window.sharedStorage.append('key', 'value')`);
  await evaluateAndLogIssue(`window.sharedStorage.delete('key')`);
  await evaluateAndLogIssue(`window.sharedStorage.clear()`);
  await evaluateAndLogIssue(
      `sharedStorage.batchUpdate([new SharedStorageSetMethod("key0", "value0")])`);
  await evaluateAndLogIssue(
      `window.sharedStorage.createWorklet('resources/shared-storage-worklet.js')`);

  testRunner.completeTest();
})
