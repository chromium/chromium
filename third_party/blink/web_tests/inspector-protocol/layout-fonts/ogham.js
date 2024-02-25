(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var page = await testRunner.createPage();
  await page.loadHTML(`
    <html>
    <meta charset="UTF-8">
    <body>
      <!-- All ogham glyps in two font families. -->
      <div class="test">
        <div id=oghammonofont></div>
        <div id=oghamdefaultfont></div>
      </div>
    </body>
    </html>
  `);
  var session = await page.createSession();
  await session.evaluate(`
    function makeCharHtml(charCode) {
      return "&#x" + charCode.toString(16) + ";";
    }
    function makeCharHtmlSequence(first, last) {
      var result = "";
      for (var i = first; i <= last; i++)
        result += makeCharHtml(i);
      return result;
    }
    charSequenceHtml = makeCharHtmlSequence(0x1680, 0x169c);
    document.getElementById("oghammonofont").innerHTML = charSequenceHtml;
    document.getElementById("oghamdefaultfont").innerHTML = charSequenceHtml;
  `);

  var helper = await testRunner.loadScript('./resources/layout-font-test.js');
  await helper(testRunner, session);
  testRunner.log('There should be two lines of Ogham above.');
  testRunner.completeTest();
})
