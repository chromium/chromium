(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <body>
        <div class="test">
            <!-- Private use area Ranges: U+E000..U+F8FF, U+F0000..U+FFFFD, U+100000..U+10FFFD -->
            <div id="pua_test_mono" style="font-family: monospace;">abc&#xE000;&#xE401;&#xE402;&#xE403;&#xF8FF;&#xF0000;&#xFAAAA;&#xFFFFD;&#x100000;&#x10AAAA;&#x10FFFD;</div>
            <div id="pua_test_serif" style="font-family: serif;">abc&#xE000;&#xE401;&#xE402;&#xE403;&#xF8FF;&#xF0000;&#xFAAAA;&#xFFFFD;&#x100000;&#x10AAAA;&#x10FFFD;</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();
  testRunner.log(`Test passes if the fallback font selected for the Unicode Private Use Area test text is the priority font.`);

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  var results = await helper(testRunner, session);
  testRunner.completeTest();
})
