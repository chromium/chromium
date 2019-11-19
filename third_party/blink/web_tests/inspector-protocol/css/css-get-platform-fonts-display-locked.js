(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
@font-face {
    font-family: 'ahem';
    src: url('${testRunner.url('./resources/Ahem.ttf')}');
}
#parent {
  contain: style layout;
  font-family: 'ahem';
  background-color: gray;
}
#parent:first-letter {
  font-family: 'Times New Roman';
  font-size: 400%;
  background-color: blue;
}

#parent:first-line {
  font-family: 'Courier New';
  background-color: yellow;
}
</style>

<div id='parent'>
7chars.<br>
<span id='child'>Some line with 29 characters.</span>
</div>
`, 'Test css.getPlatformFontsForNode method with display locking.');

  await session.evaluateAsync(async () => {
    await requestAnimationFrame(() => { document.getElementById("parent").renderSubtree = "invisible skip-activation"; });
  });

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.runTestSuite([
    async function testParent(next) {
      await platformFontsForElementWithSelector('#parent');
    },
    async function testChild(next) {
      await platformFontsForElementWithSelector('#child');
    },
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

