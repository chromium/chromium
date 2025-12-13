(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
    <script>
      window.myStyleSheet = new CSSStyleSheet();
      document.adoptedStyleSheets = [window.myStyleSheet];
    </script>
  `,
      'Test that DOM events are fired for adopted stylesheets.');

  const {result: {root}} = await dp.DOM.getDocument();
  testRunner.log(
      `There are ${root.adoptedStyleSheets.length} adopted stylesheets.`);

  testRunner.log('Removing stylesheet...');
  session.evaluate(() => {
    document.adoptedStyleSheets = [];
  });
  const msg = await dp.DOM.onceAdoptedStyleSheetsModified();
  testRunner.log(`Node IDs match: ${msg.params.nodeId === root.nodeId}`);
  testRunner.log(`There are now ${
      msg.params.adoptedStyleSheets.length} adopted stylesheets.`);

  testRunner.log('Adding stylesheet...');
  session.evaluate(() => {
    document.adoptedStyleSheets = [window.myStyleSheet];
  });
  const msg2 = await dp.DOM.onceAdoptedStyleSheetsModified();
  testRunner.log(`Node IDs match: ${msg2.params.nodeId === root.nodeId}`);
  testRunner.log(`There are now ${
      msg2.params.adoptedStyleSheets.length} adopted stylesheets.`);
  testRunner.log(`Adopted stylesheet IDs match: ${
      msg2.params.adoptedStyleSheets[0].styleSheetId ===
      root.adoptedStyleSheets[0].styleSheetId}`);
  testRunner.completeTest();
});
