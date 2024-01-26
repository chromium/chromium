(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
#test { color: green; }
</style>
<article id='test'></article>
`, 'The test verifies functionality of protocol method CSS.setStyleTexts and DOM.undo.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  let eventPromise = dp.CSS.onceStyleSheetAdded();

  await dp.DOM.enable();
  await dp.CSS.enable();

  let event = await eventPromise;
  let styleSheetId = event.params.header.styleSheetId;

  const setStyleTexts = cssHelper.setStyleTexts.bind(cssHelper, styleSheetId, false);
  const documentNodeId = await cssHelper.requestDocumentNodeId();

  let response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Initial style sheet text ====');
  testRunner.log(response.result.text);

  await setStyleTexts([{
    styleSheetId: styleSheetId,
    range: { startLine: 1, startColumn: 7, endLine: 1, endColumn: 22 },
    text: "color: blue;",
  }]);
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test');

  eventPromise = dp.CSS.onceStyleSheetAdded();
  // Clear the style content.
  await session.evaluate(fontURL => {
    const style = document.querySelector('style');
    style.textContent = '';
  });
  await dp.DOM.undo();

  event = await eventPromise;
  styleSheetId = event.params.header.styleSheetId;
  response = await dp.CSS.getStyleSheetText({styleSheetId});
  testRunner.log('==== Style sheet text after clearing the stylesheet and DOM.Undo ====');
  testRunner.log(response.result.text || '<empty>');
  testRunner.completeTest();
})
