// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that FileReader's Blob read does not emit Network.requestWillBeSent events.`);

  const requests = [];
  dp.Network.onRequestWillBeSent(event => requests.push(event.params));
  await dp.Network.enable();
  await dp.Runtime.enable();

  await session.evaluate(`
      function readBlob()
      {
          var reader = new FileReader();
          reader.onloadend = function () {
              console.log('done');
          };
          reader.readAsArrayBuffer(new Blob([ 'test' ]));
      }
      //# sourceURL=file-reader-with-network-panel.js`);

  const consolePromise = dp.Runtime.onceConsoleAPICalled();
  session.evaluate('readBlob();');
  const {params} = await consolePromise;
  const topFrame = params.stackTrace.callFrames[0];
  const shortUrl = topFrame.url.split('/').pop();

  testRunner.log('requests in the network panel: ' + requests.length);
  testRunner.log(`${shortUrl}:${topFrame.lineNumber + 1} ${
      params.args.map(a => a.value).join(' ')}`);
  testRunner.completeTest();
});
