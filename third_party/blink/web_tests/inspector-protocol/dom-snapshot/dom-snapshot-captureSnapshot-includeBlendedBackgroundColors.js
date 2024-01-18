(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('../resources/dom-snapshot-includeBlendedBackgroundColors.html', 'Tests DOMSnapshot.getSnapshot reports blended background colors of each node.');

  const response = await dp.DOMSnapshot.captureSnapshot({'computedStyles': [], 'includeBlendedBackgroundColors': true});
  if (response.error) {
    testRunner.log(response);
    return testRunner.completeTest();;
  }
  const document = response.result.documents[0];
  const strings = response.result.strings;
  const colors = document.layout.blendedBackgroundColors.filter(color => color !== -1);
  testRunner.log('Expected to find 1 blended color. Actual: ' + colors.length);
  testRunner.log('The blended color is: ' + strings[colors[0]]);
  testRunner.completeTest();
})
