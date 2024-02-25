(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // TODO(crbug.com/122303): Support generic font families added to CSS Fonts
  // Module Level 4.
  const genericFamilyNames = [
    'standard',
    'serif',
    'sansSerif',
    'cursive',
    'fantasy',
    'fixed',
    'math',
  ];

  function cssKeywordFromPreference(name) {
    switch (name) {
      case 'standard':
        return 'initial';
        break;
      case 'fixed':
        return 'monospace';
        break;
      case 'sansSerif':
        return 'sans-serif';
        break;
      default:
        return name;
        break;
    }
  }

  for (const genericFontFamily of genericFamilyNames) {
    let {session, dp} = await testRunner.startHTML(
        `<html>
          <style>
            @font-face {
              font-family: myahem;
              src: url(../../resources/Ahem.woff) format("woff");
            }
            body {
              font-family: myahem;
            }
            div.test {
              font-family: ${cssKeywordFromPreference(genericFontFamily)};
            }
          </style>
          <body>
            <div class="test">${genericFontFamily}</div>
          </body>
        </html>`,
        `Tests Page.setFontFamilies() for '${genericFontFamily}'.`);

    await dp.DOM.enable();
    await dp.CSS.enable();

    async function logPlatformFonts(selector) {
      testRunner.log(selector);
      const root = (await dp.DOM.getDocument()).result.root;
      const nodeId = (await dp.DOM.querySelector({
                       nodeId: root.nodeId,
                       selector: selector
                     })).result.nodeId;
      const fonts =
          (await dp.CSS.getPlatformFontsForNode({nodeId})).result.fonts;
      testRunner.log(fonts);
    }

    // Override generic fonts
    const fontFamilies = {};
    fontFamilies[genericFontFamily] = 'Ahem';
    await dp.Page.setFontFamilies({fontFamilies});

    // Force re-layout to make sure the font list is up-to-date.
    session.evaluate('document.body.offsetTop;');

    // Log overridden generic font.
    await logPlatformFonts('.test');
  }

  testRunner.completeTest();
})
