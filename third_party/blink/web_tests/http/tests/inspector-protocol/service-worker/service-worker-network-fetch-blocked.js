// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} =
      await testRunner.startBlank(`Tests blocking fetch in Service Workers.\n`);
  const makeFetchInServiceWorker =
      await testRunner.loadScript('resources/network-fetch-helper.js');
  await makeFetchInServiceWorker(testRunner, session, {
    scope:
        '/inspector-protocol/service-worker/resources/network-fetch-worker-blocked-scope',
    url: 'http://127.0.0.1:8000/devtools/network/resources/resource.php',
    blockedURLPatterns: ['*://*:*/*resources/resource.php'],
  });
  testRunner.completeTest();
})
