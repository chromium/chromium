(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(
      `
    <div id="template">
      <template shadowrootmode="open"><span>Hello</span></template>
    </div>
    <script>
      window.myStyleSheet = new CSSStyleSheet();
      window.shadow = document.getElementById('template').shadowRoot;
      window.shadow.adoptedStyleSheets = [window.myStyleSheet];
    </script>
  `,
      'Test that DOM events are fired for adopted stylesheets in shadow DOM.');

  const document = await dp.DOM.getDocument({depth: -1, pierce: true});
  const root =
      document.result.root.children[0].children[1].children[0].shadowRoots[0];
  testRunner.log(
      `There are ${root.adoptedStyleSheets.length} adopted stylesheets.`);

  testRunner.log('Removing stylesheet...');
  session.evaluate(() => {
    window.shadow.adoptedStyleSheets = [];
  });
  const msg = await dp.DOM.onceAdoptedStyleSheetsModified();
  testRunner.log(`Node IDs match: ${msg.params.nodeId === root.nodeId}`);
  testRunner.log(`There are now ${
      msg.params.adoptedStyleSheets.length} adopted stylesheets.`);

  testRunner.log('Adding stylesheet...');
  session.evaluate(() => {
    window.shadow.adoptedStyleSheets = [window.myStyleSheet];
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
