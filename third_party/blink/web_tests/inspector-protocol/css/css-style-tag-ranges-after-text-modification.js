(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`<style>div{color:red;}</style><div id='inspected'></div>`, 'Verify style tag reports matching styles properly after editing style content through javascript');

  var [,,styleSheetAdded] = await Promise.all([
    dp.DOM.enable(),
    dp.CSS.enable(),
    dp.CSS.onceStyleSheetAdded(),
  ]);
  var styleSheetId = styleSheetAdded.params.header.styleSheetId;

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.log('=== Initial "color" matching property value ===');
  await dumpColorProperty();

  testRunner.log('=== Modify "color" via protocol ===');
  await dp.CSS.setStyleSheetText({styleSheetId, text: 'div{color: yellow;}'});
  await dumpColorProperty();

  testRunner.log('=== Modify "color" via page javascript ===');
  await session.evaluate(() => document.querySelector('style').innerHTML = 'div{color: green;}');
  await dumpColorProperty();
  testRunner.completeTest();

  async function dumpColorProperty() {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
    var {result} = await dp.CSS.getMatchedStylesForNode({'nodeId': nodeId});
    for (var matchedRule of result.matchedCSSRules) {
      for (var property of matchedRule.rule.style.cssProperties) {
        if (property.name !== 'color')
          continue;
        testRunner.log(property);
        return;
      }
    }
  }
})
