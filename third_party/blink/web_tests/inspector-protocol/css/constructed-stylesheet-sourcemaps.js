(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test that font-palette-values rules are reported');

  await dp.DOM.enable();
  await dp.CSS.enable();

  await page.loadHTML(`
      <div id=host></div>
      <script>
      host.attachShadow({mode: 'open'});
      window.sheet = text => {
        const styles = new CSSStyleSheet();
        styles.replaceSync(text);
        host.shadowRoot.adoptedStyleSheets = [styles];
        return styles;
      };
      </script>
    `);

  async function evaluate(text) {
    await session.evaluate(text);
    const {params: {header: {sourceURL, sourceMapURL}}} =
        await dp.CSS.onceStyleSheetAdded();
    testRunner.log(`Sheet: "${text}": sourceMapURL='${
        sourceMapURL}' sourceURL='${sourceURL}'`);
  }

  await evaluate(`sheet('/*# sourceURL=constructed.css */')`);
  await evaluate(`sheet('/*# sourceMappingURL=foobar1 */')`);
  await evaluate(`sheet('/*# sourceMappingURL=foobar2 */')`);
  await evaluate(`sheet('')`);
  await evaluate(`sheet('/*# sourceMappingURL=foobar3 */')`);
  await evaluate(
      `sheet('/*# sourceMappingURL=foobar3 */').insertRule('* {color: green;}')`);
  await evaluate(
      `sheet('* {color: green;}/*# sourceMappingURL=foobar3 */').removeRule(0)`);

  testRunner.completeTest();
});
