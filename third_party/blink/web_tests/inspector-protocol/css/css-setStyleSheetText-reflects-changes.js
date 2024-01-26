(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var { page, session, dp } = await testRunner.startHTML(`
    <link rel='stylesheet' href='${testRunner.url('resources/set-stylesheet-text.css')}'>
    <div id="container"></div>`,
    'Verify that changes via CSS.setStyleSheetText are reflected in computed styles');

  async function getComputedBackgroundColor(nId) {
    const response = await dp.CSS.getComputedStyleForNode({ nodeId: nId });
    const foundStyle = response.result.computedStyle.find(style => style.name === 'background-color');
    return foundStyle.value;
  }

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var nodeId = await cssHelper.requestNodeId(documentNodeId, '#container');
  var matchedStylesResponse = await dp.CSS.getMatchedStylesForNode({ nodeId });
  var styleSheetId = matchedStylesResponse.result.matchedCSSRules[1].rule.style.styleSheetId;

  testRunner.log('Initial background-color');
  testRunner.log(await getComputedBackgroundColor(nodeId));

  await dp.CSS.setStyleSheetText({ styleSheetId, text: `#container { background-color: red; }` });
  testRunner.log('Then background-color');
  testRunner.log(await getComputedBackgroundColor(nodeId));

  testRunner.completeTest();
})
