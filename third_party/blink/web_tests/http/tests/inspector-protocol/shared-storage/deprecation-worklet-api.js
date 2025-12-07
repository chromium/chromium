// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that deprecation issues are reported for shared storage worklet APIs.
`);

  await dp.Audits.enable();

  async function evaluateAndLogIssue(js) {
    // Unmute deprecation issue reporting by navigating.
    await page.navigate(
        'https://a.test:8443/inspector-protocol/resources/empty.html');
    let promise = dp.Audits.onceIssueAdded();
    await session.evaluateAsync(js);
    let result = await promise;

    // The frameId and scriptId are dynamic, so we remove them from the output.
    delete result.params.issue.details.deprecationIssueDetails.affectedFrame
        .frameId;
    delete result.params.issue.details.deprecationIssueDetails
        .sourceCodeLocation.scriptId;
    testRunner.log(result.params, 'Inspector issue: ');
  }

  const addModule =
      `window.sharedStorage.worklet.addModule('resources/shared-storage-worklet.js')`;
  const selectURL = `${
      addModule}.then(() => {return window.sharedStorage.selectURL('my-operation', [{url: 'data:,a'}], {resolveToConfig: true})})`;

  await evaluateAndLogIssue(addModule);
  await evaluateAndLogIssue(`${
      addModule}.then(() => window.sharedStorage.worklet.run('my-operation'))`);
  await evaluateAndLogIssue(
      `${addModule}.then(() => window.sharedStorage.run('my-operation'))`);
  await evaluateAndLogIssue(selectURL);
  await evaluateAndLogIssue(`${
      addModule}.then(() => window.sharedStorage.selectURL('my-operation', [{url: 'data:,a'}]))`);

  await evaluateAndLogIssue(`${
      selectURL}.then((config) => config.setSharedStorageContext('example-context'))`);

  var f = document.createElement('fencedframe');
  document.body.appendChild(f);
  const selectURLGet = `${
      addModule}.then(() => {return window.sharedStorage.selectURL('my-operation', [{url: 'resources/deprecation-get.https.html'}], {resolveToConfig: true})})`;

  await evaluateAndLogIssue(
      `${selectURLGet}.then((config) => f.config = config)`);

  testRunner.completeTest();
})
