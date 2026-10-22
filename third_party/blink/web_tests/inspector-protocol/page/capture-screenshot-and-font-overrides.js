(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `<html>
      <style>
        body { margin: 0; }
        #family { font: 20px monospace; }
        #size { font-family: monospace; }
        #tall { height: 5000px; }
      </style>
      <body>
        <span id="family">XXXXX</span>
        <span id="size">XXXXX</span>
        <div id="tall"></div>
      </body>
    </html>`,
      'Tests that Page.setFontFamilies and Page.setFontSizes overrides are preserved across Page.captureScreenshot with captureBeyondViewport and reset by Page.disable.');

  // With the Ahem font, every glyph is a 1em square, so the five characters
  // are exactly 100px wide. Any other monospace font gives a different width.
  async function log(label) {
    const width = await session.evaluate(
        `document.getElementById('family').getBoundingClientRect().width`);
    const fontSize = await session.evaluate(
        `getComputedStyle(document.getElementById('size')).fontSize`);
    testRunner.log(`${label}: monospace resolves to Ahem: ${
        width === 100}, fixed font size: ${fontSize}`);
  }

  await log('Initial');
  await dp.Page.setFontFamilies({fontFamilies: {fixed: 'Ahem'}});
  await dp.Page.setFontSizes({fontSizes: {fixed: 30}});
  await log('After overrides');

  // Capturing beyond the viewport pushes modified web preferences to the
  // renderer and then restores the original ones.
  const response = await dp.Page.captureScreenshot({
    format: 'png',
    clip: {x: 0, y: 0, width: 1, height: 1, scale: 1},
    captureBeyondViewport: true,
  });
  testRunner.log(`Screenshot captured: ${!response.error}`);
  await log('After Page.captureScreenshot');

  await dp.Page.disable();
  await log('After Page.disable');

  testRunner.completeTest();
})
