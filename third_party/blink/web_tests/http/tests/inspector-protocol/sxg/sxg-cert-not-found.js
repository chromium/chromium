// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests the signed exchange information are available when the certificate file is not available.\n');
  const SXGHelper = await testRunner.loadScript('resources/sxg-helper.js');
  const helper = new SXGHelper(testRunner, session);
  await helper.enable();
  await helper.addIframe('/loading/sxg/resources/sxg-cert-not-found.sxg');
  helper.dumpLogEntries();
  helper.dumpRequestsWithSignedExchangeInfo();
  testRunner.completeTest();
})
