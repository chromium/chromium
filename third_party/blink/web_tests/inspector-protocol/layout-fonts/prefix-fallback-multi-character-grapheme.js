(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
     <style type="text/css">
        @font-face {
            font-family: 'Tinos';
            src: url("../../third_party/Tinos/tinos-subset.woff2");
        }

        body {
            font-family: "Tinos", Times, serif;
            font-size: 40px;
        }
    </style>
    <body>
        <div class="test">
            <div id="should_be_half_tinos_half_serif">bẩbẩbẩbẩbẩbẩbẩ</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
