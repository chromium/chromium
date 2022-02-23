(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
    `<html>
      <style>
        @font-face {
            font-family: myahem;
            src: url(../../resources/Ahem.woff) format("woff");
        }
        div.standard_font {
        }
        div.sans_serif_font {
          font-family: sans-serif;
        }
        div.fixed_font {
          font-family: fixed;
        }
      </style>
      <body>
        <div style="-webkit-locale: 'en_US'" class="standard_font">Standard</div>
        <div style="-webkit-locale: 'en_US'" class="sans_serif_font">SansSerif</div>
        <div style="-webkit-locale: 'en_US'" class="fixed_font">Fixed</div>

        <div style="-webkit-locale: 'ru_RU'" class="standard_font_ru">Standard</div>
        <div style="-webkit-locale: 'ru_RU'" class="sans_serif_font_ru">SansSerif</div>
        <div style="-webkit-locale: 'ru_RU'" class="fixed_font_ru">Fixed</div>
      </body>
    </html>`,
  'Tests Page.setFontFamilies for scripts.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  async function logPlatformFonts(selector) {
    testRunner.log(selector);
    const root = (await dp.DOM.getDocument()).result.root;
    const nodeId = (await dp.DOM.querySelector(
      {nodeId: root.nodeId, selector: selector})).result.nodeId;
    const fonts = (await dp.CSS.getPlatformFontsForNode(
      {nodeId: nodeId})).result.fonts;
    testRunner.log(fonts);
  }

  // Override generic fonts
  const result = await dp.Page.setFontFamilies({
    fontFamilies: {
    },
    forScripts: [{
      script: 'Cyrl',
      fontFamilies: {
        standard: 'Ahem',
        sansSerif: 'Ahem',
        fixed: 'Ahem'
      }
    },
    {
      script: 'Latn',
      fontFamilies: {
        standard: 'Courier',
        sansSerif: 'Courier',
        fixed: 'Courier'
      }
    }]
  });
  testRunner.log(result);

  // Log overridden Latin fonts.
  await logPlatformFonts('.standard_font');
  await logPlatformFonts('.sans_serif_font');
  await logPlatformFonts('.fixed_font');

  // ru_RU text should use Cyrillic font families.
  await logPlatformFonts('.standard_font_ru');
  await logPlatformFonts('.sans_serif_font_ru');
  await logPlatformFonts('.fixed_font_ru');

  testRunner.completeTest();
})
