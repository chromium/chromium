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
            div {
                background-color: yellow;
            }
        }
    }
  </style>
  <div></div>
`, 'Nested @starting-style with nested rule');

    let CSSHelper = await testRunner.loadScript('../../resources/css-helper.js');
    let cssHelper = new CSSHelper(testRunner, dp);

    await dp.DOM.enable();
    await dp.CSS.enable();

    let documentNodeId = await cssHelper.requestDocumentNodeId();
    let divId = (await dp.DOM.querySelector({nodeId: documentNodeId, selector: "div"})).result.nodeId;

    await cssHelper.loadAndDumpMatchingRulesForNode(divId);
    testRunner.completeTest();
});