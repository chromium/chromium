(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(`Verifies that CSS.fontsUpdated events are sent as the web font is loaded by font data buffer.`);
  await dp.DOM.enable();
  await dp.CSS.enable();
  session.evaluate(`
    const script = document.createElement('script');
    script.src = '../../resources/ahem.js';
    document.head.appendChild(script);
  `);
  event = await dp.CSS.onceFontsUpdated(
                       event => typeof event.params.font !== 'undefined' &&
                       event.params.font.fontFamily === 'Ahem');
  const font = event.params.font;
  testRunner.log(font.fontFamily);         // Ahem
  testRunner.log(font.fontStyle);          // normal
  testRunner.log(font.fontVariant);        // normal
  testRunner.log(font.fontWeight);         // normal
  testRunner.log(font.fontStretch);        // normal
  testRunner.log(font.unicodeRange);       // U+0-10FFFF
  testRunner.log(font.platformFontFamily); // Ahem
  testRunner.log(font.src);                // Empty
  testRunner.completeTest();
});
