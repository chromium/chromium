(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
  /* This style should be inherited by the ::backdrop pseudo */
  :root {
    --foo: bar;
  }
  dialog {
    color: green;
  }
  dialog::backdrop {
    opacity: 0.5;
  }
</style>
<dialog id="inspected"></dialog>
`, 'Tests that ::backdrop inherits from its originating element');

  let CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  let cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  await session.evaluate(() => document.getElementById("inspected").showModal());

  let documentNodeId = await cssHelper.requestDocumentNodeId();
  let dialogNodeId = (await dp.DOM.querySelector({nodeId: documentNodeId, selector: "#inspected"})).result.nodeId;
  let backdropNodeId = (await dp.DOM.describeNode({nodeId: dialogNodeId})).result.node.pseudoElements[0].nodeId;

  await cssHelper.loadAndDumpMatchingRulesForNode(backdropNodeId);
  testRunner.completeTest();
});
