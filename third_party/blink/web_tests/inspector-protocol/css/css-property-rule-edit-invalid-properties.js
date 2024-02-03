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
      'Test that property rules cannot be rendered invalid by edits');

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

  {
    const text = 'syntax: "<integer>"; initial-value: 0;';
    const edits = [{styleSheetId, range, text}];
    const result = await dp.CSS.setStyleTexts(
        {edits, nodeForPropertySyntaxValidation: nodes.get('div')});
    testRunner.log(result);
  }

  {
    const text = 'syntax: "<integer>"; initial-value: red; inherits: true;';
    const edits = [{styleSheetId, range, text}];
    const result = await dp.CSS.setStyleTexts(
        {edits, nodeForPropertySyntaxValidation: nodes.get('div')});
    testRunner.log(result);
  }

  {
    const text = 'syntax: "<len>"; initial-value: 0; inherits: true;';
    const edits = [{styleSheetId, range, text}];
    const result = await dp.CSS.setStyleTexts(
        {edits, nodeForPropertySyntaxValidation: nodes.get('div')});
    testRunner.log(result);
  }

  {
    const text = 'syntax: "<length>"; initial-value: 0; inherits: true;';
    const edits = [{styleSheetId, range, text}];
    const result = await dp.CSS.setStyleTexts(
        {edits, nodeForPropertySyntaxValidation: nodes.get('div')});
    testRunner.log(result);
  }

  testRunner.completeTest();
});
