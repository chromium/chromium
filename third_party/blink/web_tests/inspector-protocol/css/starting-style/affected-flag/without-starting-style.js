(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
<style>
  div {
    padding: 10px;
    transition: background-color 1s ease;
    display: inline-block;
  }
</style>
<div></div>
`, 'Element without starting-style has affectedByStartingStyles set to false');

  let CSSHelper = await testRunner.loadScript('../../../resources/css-helper.js');
  let cssHelper = new CSSHelper(testRunner, dp);

  function affectedByStartingStyles(description) {
    return 'affectedByStartingStyles' in description.result.node && description.result.node.affectedByStartingStyles;
  }

  await dp.DOM.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const divId = (await dp.DOM.querySelector({nodeId: documentNodeId, selector: "div"})).result.nodeId;

  let divDescription = await dp.DOM.describeNode({nodeId: divId});
  testRunner.log(affectedByStartingStyles(divDescription));

  testRunner.completeTest();
});
