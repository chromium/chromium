(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `<style>
@property --n {
  syntax: "<integer>";
  initial-value: 0;
  inherits: false;
}
</style>
<span class="foo">test
  `,
      'Test property rule edits with invalid or omitted descriptors');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();

  const nodes = new Map();
  function collectNodes(node) {
    nodes.set(node.localName, node.nodeId);
    node.children?.forEach(collectNodes);
    node.pseudoElements?.forEach(collectNodes);
  }
  collectNodes(root);

  const {result: {cssPropertyRules}} =
      await dp.CSS.getMatchedStylesForNode({nodeId: nodes.get('body')});
  const {styleSheetId, range} = cssPropertyRules[0].style;
  const {result: {text: originalText}} =
      await dp.CSS.getStyleSheetText({styleSheetId});

  async function applyEdit(text) {
    const edits = [{styleSheetId, range, text}];
    const result = await dp.CSS.setStyleTexts(
        {edits, nodeForPropertySyntaxValidation: nodes.get('div')});
    testRunner.log(result);
    // Restore the original text so each edit starts from the same state.
    await dp.CSS.setStyleSheetText({styleSheetId, text: originalText});
  }

  await applyEdit('syntax: "<integer>"; initial-value: 0;');
  await applyEdit('syntax: "<integer>"; initial-value: red; inherits: true;');
  await applyEdit('syntax: "<len>"; initial-value: 0; inherits: true;');
  await applyEdit('syntax: "<length>"; initial-value: 0; inherits: true;');

  testRunner.completeTest();
});
