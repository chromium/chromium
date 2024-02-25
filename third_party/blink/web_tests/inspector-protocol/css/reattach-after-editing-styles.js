(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/reattach-after-editing-styles.css')}'/>
<div id='test'></div>
`, 'This test checks that styles edited through inspector are correctly shown upon reattach.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();
  await dp.Page.enable()

  var event = await dp.CSS.onceStyleSheetAdded();
  var originalStylesheetId = event.params.header.styleSheetId;
  var documentNodeId = await cssHelper.requestDocumentNodeId();

  var response = await dp.Page.getResourceTree();
  var frameId = response.result.frameTree.frame.id;

  var setStyleTexts = cssHelper.setStyleTexts.bind(cssHelper, originalStylesheetId, false);

  testRunner.log('\n==== Original: Matching rules for #test ====');
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');

  testRunner.log('\nCSS.setStyleTexts(...) to existing stylesheet');
  await setStyleTexts([{
        styleSheetId: originalStylesheetId,
        range: { startLine: 0, startColumn: 7, endLine: 2, endColumn: 0 },
        text: "\n    content: 'EDITED';\n",
      }]);

  testRunner.log('\nCSS.addRule(...) to new stylesheet');
  var response = await dp.CSS.createStyleSheet({frameId: frameId});
  var newStylesheetId = response.result.styleSheetId;
  await cssHelper.addRule(newStylesheetId, false, {
    location: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 0 },
    ruleText: '#inspected, #test { height: 1 }\n',
  });

  testRunner.log('\n==== Modified: Matching rules for #test ====');
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');

  testRunner.log('\nDisconnecting devtools');
  await session.disconnect();

  testRunner.log('\nReattaching devtools');
  session = await page.createSession();
  var dp = session.protocol;
  var cssHelper = new CSSHelper(testRunner, dp);
  dp.DOM.enable();
  dp.CSS.enable();
  var documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.log('\n==== Reattached: Matching rules for #test ====');
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');
  testRunner.completeTest();
})
