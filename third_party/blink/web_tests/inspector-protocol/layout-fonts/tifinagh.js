(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <div class="test">
        <!-- Universal declaration of Human Rights written in Central
             Atlas Tamazight using Tifinagh script. -->
        <div id="tifinagh_text">ⵉⵎⴷⴰⵏⴻⵏ, ⴰⴽⴽⴻⵏ ⵎⴰ ⵍⵍⴰⵏ ⵜⵜⵍⴰⵍⴻⵏ ⴷ ⵉⵍⴻⵍⵍⵉⵢⴻⵏ ⵎⵙⴰⵡⴰⵏ ⴷⵉ ⵍⵃⵡⴻⵕⵎⴰ ⴷ ⵢⵉⵣⴻⵔⴼⴰⵏ-ⵖⵓⵔ ⵙⴻⵏ ⵜⴰⵎⵙⴰⴽⵡⵉⵜ ⴷ ⵍⴰⵇⵓⴻⵍ ⵓ ⵢⴻⵙⵙⴻⴼⴽ ⴰⴷ-ⵜⵉⵍⵉ ⵜⴻⴳⵎⴰⵜⵜ ⴳⴰⵔ ⴰⵙⴻⵏ</div>
    </html>
  `);
  var session = await page.createSession();

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.completeTest();
})
