(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
<style>
  body {
    div {
      padding: 10px;
      transition: background-color 1s ease;
      display: inline-block;
    }

    @starting-style {
      > div {
        background-color: yellow;
      }
    }
  }
</style>
<div><div></div></div>
`, 'Nested @starting-style with tigther selector and nested divs');

  let CSSHelper = await testRunner.loadScript('../../../resources/css-helper.js');
  let cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  let documentNodeId = await cssHelper.requestDocumentNodeId();
  let firstDivId = (await dp.DOM.querySelector({nodeId: documentNodeId, selector: "div"})).result.nodeId;
  let secondDivId = (await dp.DOM.querySelector({nodeId: documentNodeId, selector: "div div"})).result.nodeId;

  await cssHelper.loadAndDumpMatchingRulesForNode(firstDivId);
  await cssHelper.loadAndDumpMatchingRulesForNode(secondDivId);
  testRunner.completeTest();
});
