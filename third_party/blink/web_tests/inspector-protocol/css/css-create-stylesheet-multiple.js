(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const { dp } = await testRunner.startHTML(`
    <div id='inspected'>Inspected contents</div>
  `, 'Tests that rules could be added to multiple new stylesheets.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  await dp.Page.enable();
  const response = await dp.Page.getResourceTree();
  const frameId = response.result.frameTree.frame.id;

  await dp.CSS.enable();
  dp.CSS.onStyleSheetAdded(event => {
    const header = event.params.header;
    const urlString = header.sourceURL ? ' (' + cssHelper.displayName(header.sourceURL) + ')' : '';
    testRunner.log('Style sheet added: ' + header.origin + urlString);
  });

  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#inspected');

  const ids = new Set();
  async function testStylesheet(className, opts) {
    testRunner.log('Creating inspector stylesheet.');
    const response = await dp.CSS.createStyleSheet(opts);
    testRunner.log('Adding a rule to the inspector stylesheet.');
    ids.add(response.result.styleSheetId);
    await cssHelper.addRule(response.result.styleSheetId, false, {
      location: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 0 },
      ruleText: `#inspected.${className} {}\n`,
    });
    await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#inspected');
  }

  // Without force=true, createStylesheet updates an existing one.
  await testStylesheet('s1', { frameId });
  await testStylesheet('s1-updated', { frameId });
  await testStylesheet('s2', { frameId, force: true });
  await testStylesheet('s3', { frameId, force: true });

  testRunner.log('Count of unique ids: ' + ids.size);

  testRunner.completeTest();
});
