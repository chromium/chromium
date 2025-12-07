// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  await testRunner.startBlank(
      `Test creating hidden targets with invalid params`);

  testRunner.log('Create hidden target');

  testRunner.log(await testRunner.browserP().Target.createTarget(
      {url: 'about:blank?HIDDEN=TARGET', hidden: true, background: false}));

  testRunner.log(await testRunner.browserP().Target.createTarget(
      {url: 'about:blank?HIDDEN=TARGET', hidden: true, newWindow: true}));

  testRunner.log(await testRunner.browserP().Target.createTarget(
      {url: 'about:blank?HIDDEN=TARGET', hidden: true, forTab: true}));

  testRunner.completeTest();
})
