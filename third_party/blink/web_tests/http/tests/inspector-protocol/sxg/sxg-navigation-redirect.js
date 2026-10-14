// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests the signed exchange information are available when the navigation succeeded after redirect.\n');
  const SXGHelper = await testRunner.loadScript('resources/sxg-helper.js');
  const helper = new SXGHelper(testRunner, session);
  await helper.enable();
  const url =
      'http://localhost:8000/resources/redirect.php?url=' +
      encodeURIComponent(
          'http://127.0.0.1:8000/loading/sxg/resources/sxg-location.sxg');
  await helper.addIframe(url);
  helper.dumpLogEntries();
  helper.dumpRequestsWithSignedExchangeInfo();
  testRunner.completeTest();
})
