// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
    `
    <div id="x" class="container">
      <p style="color: red">Text</p>
      <script>var foo;</script>
    </div>
    `, 'Tests capturing MHTML snapshots.');

  await dp.Page.enable();

  testRunner.log(`\nCapturing without specified format:`);
  testRunner.log(formatResult(await dp.Page.captureSnapshot()));

  testRunner.log(`\nCapturing with format: foo`);
  testRunner.log(formatResult(await dp.Page.captureSnapshot({format: 'foo'})));
  testRunner.completeTest();

  function formatResult(result) {
    const data = result.result ? result.result.data : null;
    if (!data)
      return result;

    const ignoredPrefixes = [
      'Snapshot-Content-Location: ',
      'Subject: ',
      'Date: ',
      'MIME-Version: ',
      'boundary="----MultipartBoundary--',
      '------MultipartBoundary--',
      'Content-ID: ',
      'Content-Transfer-Encoding: ',
      'Content-Location: ',
    ];
    let cleanData = '';
    for (const line of data.split('\n')) {
      let cleanLine = line;
      for (const prefix of ignoredPrefixes) {
        if (line.trim().startsWith(prefix)) {
          cleanLine = `<${prefix}>`;
          continue;
        }
      }
      cleanData += cleanLine;
    }
    return cleanData;
  }
})
