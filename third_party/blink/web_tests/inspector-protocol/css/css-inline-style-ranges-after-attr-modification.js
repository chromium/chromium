(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`<div id='inspected'></div>`, 'Verify inline style reports proper ranges after attr modification');

  await dp.DOM.enable();
  await dp.CSS.enable();

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  var documentNodeId = await cssHelper.requestDocumentNodeId();
  testRunner.log('=== Initial inline style ===');
  await dumpInlineStyle();

  testRunner.log('=== CSSOM-modified inline style ===');
  await session.evaluate(() => document.getElementById('inspected').style.color='blue');
  await dumpInlineStyle();
  testRunner.completeTest();

  async function dumpInlineStyle() {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');
    var {result} = await dp.CSS.getInlineStylesForNode({'nodeId': nodeId});
    testRunner.log(result.inlineStyle);
  }
})
