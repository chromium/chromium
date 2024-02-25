(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {session, dp} = await testRunner.startHTML(
    `<html>
      <style>
        @font-face {
            font-family: myahem;
            src: url(../../resources/Ahem.woff) format("woff");
        }
        body {
          font-family: myahem;
        }
        div.standard_font, div.standard_font_ru {
          font-family: initial;
        }
        div.sans_serif_font, div.sans_serif_font_ru {
          font-family: sans-serif;
        }
        div.fixed_font, div.fixed_font_ru {
          font-family: monospace;
        }
        div.notgeneric_font, div.notgeneric_font_ru {
          font-family: fixed;
        }
      </style>
      <body>
        <div style="-webkit-locale: 'en_US'" class="inherited_font">Inherited</div>
        <div style="-webkit-locale: 'ru_US'" class="inherited_font_ru">Inherited</div>
        <div style="-webkit-locale: 'en_US'" class="standard_font">Standard</div>
        <div style="-webkit-locale: 'en_US'" class="sans_serif_font">SansSerif</div>
        <div style="-webkit-locale: 'en_US'" class="fixed_font">Fixed</div>
        <div style="-webkit-locale: 'en_US'" class="notgeneric_font">NotGeneric</div>

        <div style="-webkit-locale: 'ru_RU'" class="standard_font_ru">Standard</div>
        <div style="-webkit-locale: 'ru_RU'" class="sans_serif_font_ru">SansSerif</div>
        <div style="-webkit-locale: 'ru_RU'" class="fixed_font_ru">Fixed</div>
        <div style="-webkit-locale: 'ru_RU'" class="notgeneric_font_ru">NotGeneric</div>
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
    const fonts = (await dp.CSS.getPlatformFontsForNode({nodeId})).result.fonts;
    testRunner.log(fonts);
  }

  // Override generic fonts
  // This uses a different family name for the standard font, so that one can
  // determine whether a font-family value is treated as a generic name.
  const result = await dp.Page.setFontFamilies({
    fontFamilies: {
    },
    forScripts: [{
      script: 'Cyrl',
      fontFamilies: {
        standard: 'Courier',
        sansSerif: 'Ahem',
        fixed: 'Ahem',
        monospace: "Times", // Ignored, not a valid property of FontFamilies.
      }
    },
    {
      script: 'Latn',
      fontFamilies: {
        standard: 'Ahem',
        sansSerif: 'Courier',
        fixed: 'Courier', // Corresponds to the "monospace" font-family.
        monospace: "Times", // Ignored, not a valid property of FontFamilies.
      }
    }]
  });
  testRunner.log(result);

  // Force re-layout to make sure the font list is up-to-date.
  session.evaluate('document.body.offsetTop;');

  // Log the custom Ahem font inherited from the body.
  await logPlatformFonts('.inherited_font');
  await logPlatformFonts('.inherited_font_ru');

  // Log overridden Latin fonts.
  await logPlatformFonts('.standard_font');
  await logPlatformFonts('.sans_serif_font');
  await logPlatformFonts('.fixed_font');

  // ru_RU text should use Cyrillic font families.
  await logPlatformFonts('.standard_font_ru');
  await logPlatformFonts('.sans_serif_font_ru');
  await logPlatformFonts('.fixed_font_ru');

  // Log the overridden standard font as a fallback (assuming no font with name
  // "fixed" is available on the system).
  await logPlatformFonts('.notgeneric_font');
  await logPlatformFonts('.notgeneric_font_ru');

  testRunner.completeTest();
})
