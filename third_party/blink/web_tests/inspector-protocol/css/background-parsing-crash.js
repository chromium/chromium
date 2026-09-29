// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} =
      await testRunner.startHTML(`
      <style>
      .absent {
          background: #fff url(foo.png) no-repeat left 4px;
      }

      body {
          background: #fff;
      }
      </style>
    `,
                                 'This test passes if it doesn\'t ASSERT.');

  await dp.DOM.enable();
  await dp.CSS.enable();
  testRunner.completeTest();
});
