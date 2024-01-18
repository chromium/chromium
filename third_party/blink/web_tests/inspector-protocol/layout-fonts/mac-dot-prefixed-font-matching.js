(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <style>
      @font-face {
        font-family: match_unique;
        font-size: 17px;
        src: local(".NewYork-Regular");
      }
      #times_local_unique {
        font-family: match_unique;
      }
      #times_family {
        font-family: ".New York";
      }
    </style>
    <body>
      <div class="test">
        <div id="times_local_unique">test</div>
        <div id="times_family">test</div>
      </div>
    </body>
    </html>
  `);
  var session = await page.createSession();
  testRunner.log('Test passes if text elements are rendered using Times font.');
  testRunner.log(`Note: Dot prefixed font names cause error console output when passed to certain CoreText font instantiation methods. Hence we want to filter them on the application side. That's why the text in the test should be displayed in the Times font.`);

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  var results = await helper(testRunner, session);

  var times_local_unique = results.find(x => x.selector === '#times_local_unique').usedFonts;
  var times_family = results.find(x => x.selector === '#times_family').usedFonts;
  var passed = (times_local_unique[times_local_unique.length - 1].familyName == 'Times'
    && times_family[times_family.length - 1].familyName == 'Times');
  testRunner.log(passed ? 'PASS' : 'FAIL');
  testRunner.completeTest();
})
