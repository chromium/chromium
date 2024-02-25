(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <style>
    .test {
      font-family: Segoe UI;
      font-size: 20pt;
    }
    #regular-weight {
      font-weight: 400;
    }
    #semilight-weight {
      font-weight: 350;
    }
    </style>
    <div class="test">
      <div id="regular-weight">Text should be in the regular Segoe UI font.</div>
      <div id="semilight-weight">Text should be Segoe UI Semilight on Win10+, else Segoe UI Light.</div>
    </div>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
