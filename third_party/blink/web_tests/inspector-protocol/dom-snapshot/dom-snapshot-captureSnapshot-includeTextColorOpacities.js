(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('../resources/dom-snapshot-includeTextColorOpacities.html', 'Tests DOMSnapshot.getSnapshot reports blended background colors of each node.');

  const response = await dp.DOMSnapshot.captureSnapshot({'computedStyles': [], 'includeTextColorOpacities': true});
  if (response.error) {
    testRunner.log(response);
    return testRunner.completeTest();;
  }
  const document = response.result.documents[0];
  testRunner.log(document.layout.textColorOpacities);
  testRunner.completeTest();
})
