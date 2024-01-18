// dom-get-document-in-limited-quirks-mode-test
(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('./resources/dom-get-document-in-limited-quirks-mode-test.html', 'Tests how DOM.getDocument reports limited quirks mode.');

  const response = await dp.DOM.getDocument({depth: 1});
  testRunner.log(response);
  testRunner.completeTest();
})
