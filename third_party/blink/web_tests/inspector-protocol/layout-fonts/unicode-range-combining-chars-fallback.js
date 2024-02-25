(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
     <style type="text/css">
        /* fallback */
        @font-face {
            font-family: 'Tinos';
            font-style: normal;
            font-weight: 700;
            src: url("../../third_party/Tinos/tinos-bold.ttf");
        }

        /* latin */
        @font-face {
            font-family: 'Tinos';
            font-style: normal;
            font-weight: 700;
            src: url("../../third_party/Tinos/tinos-subset.woff2");
            unicode-range: U+0000-00FF;
        }

        body {
            position: relative;
            font-family: "Tinos", Times, serif;
            font-weight: 600;
            font-size: 80px;
        }
    </style>
    <body>
        <div class="test">
            <div id="should_be_using_only_tinos">ẽĩj̃j́m̃p̃q̃s̃t̃ũỹẼĨJ̃J́M̃P̃Q̃S̃T̃ŨỸ¿</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
