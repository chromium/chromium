(async function(testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <style>
    </style>
    <body>
        <div class="test">
            <!-- Use a different font size in order to avoid caching the FontDescriptions and skipping matching. -->
            <div id="mixed_case_1" style="font-family: HIragINO KakU GothiC ProN; font-size: 12px;">クローミアム</div>
            <div id="mixed_case_2" style="font-family: hiRAGino kAKu gOTHIc pROn; font-size: 13px;">クローミアム</div>
            <div id="all_lower_case" style="font-family: hiragino kaku gothic pron; font-size: 14px;">クローミアム</div>
            <div id="all_upper_case" style="font-family: HIRAGINO KAKU GOTHIC PRON; font-size: 15px;">クローミアム</div>
            <div id="exact_case" style="font-family: Hiragino Kaku Gothic ProN; font-size: 16px;">クローミアム</div>
        </div>
    </body>
    </html>
  `);
  var session = await page.createSession();
  testRunner.log('Test passes if each of the test divs uses the Hiragino Kaku Gothic ProN ' +
                 'independently of capitalization of the font family name..');

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  var results = await helper(testRunner, session);

  var passed = results.length == 5;
  const reduce_match_regexp = (passed, currentValue) => {
    var current_value_passes = /Hiragino Kaku Gothic ProN/.test(currentValue.usedFonts[0].familyName)
        && currentValue.usedFonts[0].glyphCount == 6;
    return passed && current_value_passes;
  }
  passed = passed && results.reduce(reduce_match_regexp);

  testRunner.log(passed ? 'PASS' : 'FAIL');
  testRunner.completeTest();
})
