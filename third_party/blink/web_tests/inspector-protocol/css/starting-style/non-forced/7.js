(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {dp} = await testRunner.startHTML(`
  <style>
    body {
      > div {
        padding: 10px;
        transition: color 1s ease, background-color 1s ease;
        display: inline-block;

        @media (min-width: 1px) {
          @starting-style {
            --foo: yes;
            background-color: yellow;
            color: hotpink;
          }
        }
      }
    }
  </style>
  <div></div>
  `, '@starting-style with custom property');

    let CSSHelper = await testRunner.loadScript('../../../resources/css-helper.js');
    let cssHelper = new CSSHelper(testRunner, dp);

    await dp.DOM.enable();
    await dp.CSS.enable();

    let documentNodeId = await cssHelper.requestDocumentNodeId();
    let dialogNodeId = (await dp.DOM.querySelector({nodeId: documentNodeId, selector: "div"})).result.nodeId;

    await cssHelper.loadAndDumpMatchingRulesForNode(dialogNodeId);
    testRunner.completeTest();
});