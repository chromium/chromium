(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
@font-face {
    font-family: 'ahem';
    src: url('${testRunner.url('./resources/Ahem.ttf')}');
}

    #fancy {
        font-family: 'ahem';
        background-color: gray;
    }
    #fancy:first-letter {
        font-family: 'Times New Roman';
        font-size: 400%;
        background-color: blue;
    }

    #fancy:first-line {
        font-family: 'Courier New';
        background-color: yellow;
    }
</style>

<div id='fancy'>
7chars.<br>
Some line with 29 characters.
</div>
<select>
    <option>Short</option>
    <option selected>Option with a lot of chars.</option>
</select>
`, 'Test css.getPlatformFontsForNode method.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.runTestSuite([
    async function testFirstLetterPseudoClass(next) {
      await platformFontsForElementWithSelector('#fancy');
    },

    async function testSelectElementPlatformFonts(next) {
      await platformFontsForElementWithSelector('select');
    }
  ]);

  async function platformFontsForElementWithSelector(selector) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, selector);
    var response = await dp.CSS.getPlatformFontsForNode({ nodeId });
    var fonts = response.result.fonts;
    fonts.sort((a, b) => b.glyphCount - a.glyphCount);
    for (var i = 0; i < fonts.length; ++i)
      fonts[i].familyName = '<Some-family-name-' + i + '>';
    for (var i = 0; i < fonts.length; ++i) {
      var lines = [
        'Font #' + i,
        '    name: ' + fonts[i].familyName,
        '    glyphCount: ' + fonts[i].glyphCount,
        '    isCustomFont: ' + fonts[i].isCustomFont
      ];
      testRunner.log(lines.join('\n'));
    }
  }
});
