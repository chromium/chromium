// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      `Tests single resource search in inspector page agent with non existing resource url does not cause a crash.`);

  await dp.Page.enable();
  const {result: {frameTree}} = await dp.Page.getFrameTree();

  // This file should not match search query.
  const query = 'searchTest' +
      'UniqueString';
  const response = await dp.Page.searchInResource({
    frameId: frameTree.frame.id,
    url: testRunner.url('resources/non-existing.js'),
    query,
  });
  testRunner.log(response.error ? response.error.message :
                                  'FAIL: expected an error');
  testRunner.completeTest();
});
