// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const testCases = [
    [
      'fetch',
      `fetch('/', {sharedStorageWritable: true})`,
    ],
    [
      '<img>', `document.body.innerHTML = '<img src="/" sharedstoragewritable>'`
    ],
    [
      '<iframe>',
      `document.body.innerHTML = '<iframe src="/" sharedstoragewritable></iframe>'`
    ],
  ];

  const {dp, page, session} = await testRunner.startBlank(
      'Tests that sharedStorageWritable attribute is deprecated.\n');
  await dp.Audits.enable();

  for (const [name, script] of testCases) {
    // At most one deprecation issue is reported per type per page, so
    // re-navigate to ensure that this limit is not reached.
    await page.navigate('/');

    session.evaluate(script);

    let issue;
    do {
      issue = await dp.Audits.onceIssueAdded();
    } while (issue.params.issue.code !== 'DeprecationIssue' ||
             issue.params.issue.details.deprecationIssueDetails.type !==
                 'SharedStorage');

    testRunner.log(name);
  }

  testRunner.completeTest();
})
