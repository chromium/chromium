(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <body>
        <div class="test">
            <div lang="my" id="myanmar">‘ေရွးျမန္မာမင္းေတြလက္ထက္က</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();
  testRunner.log(`Test passes if a maxmium of the two first glyphs are notdef's (for Myanmar fonts that do not combine a left quote with a Myanmar spacing mark and the rest of the run is shaped, given a system Myanmar font is available.`);

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  var results = await helper(testRunner, session);

  var myanmar = results.find(x => x.selector === '#myanmar').usedFonts;
  var passed = (myanmar.length === 1 && myanmar[0].familyName.includes('Myanmar')) ||
      (myanmar.length === 2 && myanmar[0].glyphCount === 2 && myanmar[1].glyphCount > 10 && myanmar[1].familyName.includes('Myanmar'));
  testRunner.log(passed ? 'PASS' : 'FAIL');
  testRunner.completeTest();
})
