(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <style>
    .test {
      font-family: system-ui;
    }
    #system-ui-20pt {
      font-size: 20pt;
    }
    </style>
    <div class="test">
      <div id="system-ui">This text should use the system font.</div>
      <div id="system-ui-20pt">This text should use the system font.</div>
    </div>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
