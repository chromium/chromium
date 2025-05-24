(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
<style>
  div {
    padding: 10px;
    transition: background-color 1s ease;
    display: inline-block;

    @starting-style {
      background-color: yellow;
    }
  }
</style>
<div></div>
`, 'rule with nested @starting-style rule that has bare declarations');

  let CSSHelper = await testRunner.loadScript('../../../resources/css-helper.js');
  let cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  let documentNodeId = await cssHelper.requestDocumentNodeId();
  let divId = (await dp.DOM.querySelector({nodeId: documentNodeId, selector: "div"})).result.nodeId;

  await dp.CSS.forceStartingStyle({nodeId: divId, forced: true });
  await cssHelper.loadAndDumpMatchingRulesForNode(divId);

  await dp.CSS.forceStartingStyle({nodeId: divId, forced: false });
  await cssHelper.loadAndDumpMatchingRulesForNode(divId);

  testRunner.completeTest();
});
