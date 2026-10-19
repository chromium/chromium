// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that Page.getResourceTree reports lastModified and contentSize for a dynamically added resource.`);

  const url =
      testRunner.url('resources/script-with-constant-last-modified.php');
  await session.evaluateAsync(url => new Promise(resolve => {
                                const script = document.createElement('script');
                                script.type = 'text/javascript';
                                script.src = url;
                                script.onload = resolve;
                                document.body.appendChild(script);
                              }),
                              url);

  const {frameTree} = (await dp.Page.getResourceTree()).result;
  const resource = frameTree.resources.find(r => r.url === url);
  if (!resource) {
    testRunner.log('ERROR: Failed to find resource.');
    testRunner.completeTest();
    return;
  }
  const lastModified = resource.lastModified ?
      new Date(resource.lastModified * 1000).toISOString() :
      null;
  testRunner.log('Last modified: ' + lastModified);
  testRunner.log('Content size: ' + resource.contentSize);

  testRunner.completeTest();
});
