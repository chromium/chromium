// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {dp} = await testRunner.startURL(
      '../resources/blank.html',
      'Tests that Command Line API inspect() works correctly');

  await dp.Runtime.enable();

  const [{params: {object}}] = await Promise.all([
    dp.Runtime.onceInspectRequested(),
    dp.Runtime.evaluate({
      expression: 'inspect($(\'body\'))',
      includeCommandLineAPI: true,
    }),
  ]);

  testRunner.log(object);
  testRunner.completeTest();
});
