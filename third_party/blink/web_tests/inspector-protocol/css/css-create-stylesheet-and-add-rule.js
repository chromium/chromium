(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' type='text/css' href='${testRunner.url('resources/stylesheet.css')}'></link>
<div id='inspected'>Inspected contents</div>
  `, 'Tests that rules could be added to the existing and newly created stylesheets.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  await dp.Page.enable();
  var response = await dp.Page.getResourceTree();
  var frameId = response.result.frameTree.frame.id;

  dp.CSS.enable();
  dp.CSS.onStyleSheetAdded(event => {
    var header = event.params.header;
    var urlString = header.sourceURL ? ' (' + cssHelper.displayName(header.sourceURL) + ')' : '';
    testRunner.log('Style sheet added: ' + header.origin + urlString);
  });

  var styleSheetHeader = (await dp.CSS.onceStyleSheetAdded()).params.header;
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#inspected');

  testRunner.log('Adding a rule to the existing stylesheet.');
  await cssHelper.addRule(styleSheetHeader.styleSheetId, false, {
    location: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 0 },
    ruleText: '#inspected {}',
  });

  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#inspected');

  testRunner.log('Creating inspector stylesheet.');
  var response = await dp.CSS.createStyleSheet({frameId: frameId});

  testRunner.log('Adding a rule to the inspector stylesheet.');
  await cssHelper.addRule(response.result.styleSheetId, false, {
    location: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 0 },
    ruleText: '#inspected {}',
  });
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#inspected');

  testRunner.completeTest();
});

