// crbug.com/430544817
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
  <div>
    <style>
      @scope {
        #example {
          color: red;
          .nested {
          }
        }
      }
    </style>
    <style>
      @scope {
        #example {
          color: red;
          .nested {
          }
        }
      }
    </style>
    <div id="example">
      Example
    </div>
  </div>
`,
'The test verifies functionality of protocol method CSS.getMatchedStylesForNode for implicit @scope.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.log("Style rules within duplicate implicit @scope rules do appear");
  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#example');

  testRunner.completeTest();
});
