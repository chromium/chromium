(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`<link rel='stylesheet'>some text`, 'Verifies that CSS.fontsUpdated events are sent as the web font is loaded.');

  await dp.DOM.enable();
  await dp.CSS.enable();
  session.evaluate(fontURL => {
    var link = document.querySelector('link');
    link.href = fontURL;
  }, testRunner.url('./resources/noto-mono.css'));
  event = await dp.CSS.onceFontsUpdated(
                       event => typeof event.params.font !== 'undefined' &&
                       event.params.font.fontFamily === 'Noto Mono');
  var font = event.params.font;
  var fontFamily = font.fontFamily;
  var fontStyle = font.fontStyle;
  var fontVariant = font.fontVariant;
  var fontWeight = font.fontWeight;
  var fontDisplay = font.fontDisplay;
  var fontStretch = font.fontStretch;
  var unicodeRange = font.unicodeRange;
  var src = font.src;
  var platformFontFamily = font.platformFontFamily;

  testRunner.log(fontFamily);         // Noto Mono
  testRunner.log(fontStyle);          // normal (default)
  testRunner.log(fontVariant);        // normal (default)
  testRunner.log(fontWeight);         // normal (default)
  testRunner.log(fontStretch);        // normal (default)
  testRunner.log(fontDisplay);        // auto (default)
  testRunner.log(unicodeRange);       // U+0-10FFFE
  testRunner.log(platformFontFamily); // ಠ_ಠNoto Monoಠ_ಠ

  // src will be an absolute directory (machine variant), so check
  // file name but not path.
  if (!src.endsWith('NotoMono-Regular.subset.ttf')) {
    testRunner.fail('font src attribute incorrect.');
  }
  testRunner.log('SUCCESS: CSS.FontsUpdated events received.');
  testRunner.completeTest();
});
