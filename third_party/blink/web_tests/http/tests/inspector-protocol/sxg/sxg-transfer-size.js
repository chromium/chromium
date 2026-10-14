// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session} = await testRunner.startBlank(
      'Tests the transfer size of signed exchange is set correctly.\n');
  const SXGHelper = await testRunner.loadScript('resources/sxg-helper.js');
  const helper = new SXGHelper(testRunner, session);
  await helper.enable();
  const innerResponseFinished =
      helper.waitForRequestFinished(/loading\/sxg\/resources\/inner-url.html/);
  await helper.addIframe('/loading/sxg/resources/sxg-larger-than-10k.sxg');
  await innerResponseFinished;
  helper.dumpLogEntries();
  helper.dumpRequestsWithSignedExchangeInfo();
  const requests = helper.requests().filter(
      request => /sxg-larger-than-10k.sxg/.test(request.url));
  testRunner.log(`Number of outer requests: ${requests.length}`);
  testRunner.log(`Transfer size is larger than 10000: ${
      requests[0].transferSize > 10000}`);
  testRunner.completeTest();
})
