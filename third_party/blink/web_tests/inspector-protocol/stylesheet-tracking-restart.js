(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('This test checks that if style sheet is removed between two inspector launches it is not reported to frontend.');
  var page = await testRunner.createPage();
  var session = await page.createSession();
  await session.evaluate(`
    function createStyleSheet(textContent)
    {
        var styleElement = document.createElement('style');
        styleElement.textContent = textContent;
        document.head.appendChild(styleElement);
        return styleElement;
    }
    window.styleElement1 = createStyleSheet('body.class1 { color: red; } \\n /*# sourceURL=foo.css */');
    window.styleElement2 = createStyleSheet('body.class2 { color: green; } \\n /*# sourceURL=bar.css */');
  `);
  testRunner.log('\nRunning test');
  testRunner.log('Opening front-end for the first time');
  await runTest(session);
  testRunner.log('Closing inspector.');
  testRunner.log('\nRemoving style sheet.\n');
  session.evaluate('Promise.resolve().then(() => { document.head.removeChild(styleElement1); document.body.offsetWidth; })');
  await session.disconnect();
  testRunner.log('Reopening inspector.');
  session = await page.createSession();
  await session.evaluateAsync('new Promise(f => setTimeout(f, 0))');
  testRunner.log('Running test');
  testRunner.log('Opening front-end second time');
  await runTest(session);
  testRunner.completeTest();
  async function runTest(session) {
    var headersAdded = [];
    session.protocol.CSS.onStyleSheetAdded(response => headersAdded.push(response.params.header));
    var headersRemoved = [];
    session.protocol.CSS.onStyleSheetRemoved(response => headersRemoved.push(response.params.styleSheetId));
    testRunner.log('Enabling CSS domain.');
    session.protocol.DOM.enable();
    await session.protocol.CSS.enable();
    var headers = {};
    headersAdded.sort((a, b) => a.sourceURL.localeCompare(b.sourceURL));
    for (var header of headersAdded) {
      headers[header.styleSheetId] = header.sourceURL;
      testRunner.log(' - style sheet added: ' + header.sourceURL);
    }
    headersRemoved.sort();
    for (var styleSheetId of headersRemoved)
      testRunner.log(' - style sheet removed: ' + headers[styleSheetId]);
  }
})
