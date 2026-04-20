(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `<style>
         @mixin --test() {
           @result {
             @contents;
           }
         }
         body {
           background: red;

           @apply --test {
             background: cyan;
           }
         }
       </style>`,
      'The test verifies that fetching styles for an element using mixins and contents does not crash.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const bodyNodeId = await cssHelper.requestNodeId(documentNodeId, 'body');

  testRunner.log('Fetching styles for body...');
  const response = await dp.CSS.getMatchedStylesForNode({nodeId: bodyNodeId});
  if (response.error) {
    testRunner.log('Error fetching styles: ' +
                   JSON.stringify(response.error, null, 2));
  } else {
    testRunner.log('Successfully fetched styles.');
  }

  testRunner.completeTest();
});
