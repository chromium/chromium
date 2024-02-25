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
        div.standard_font {
          font-family: initial;
        }
        div.sans_serif_font {
          font-family: sans-serif;
        }
        div.fixed_font {
          font-family: monospace;
        }
        div.notgeneric_font {
          font-family: fixed;
        }
      </style>
      <body>
        <div class=inherited_font>Inherited</div>
        <div class=standard_font>Standard</div>
        <div class=sans_serif_font>SansSerif</div>
        <div class=fixed_font>Fixed</div>
        <div class=notgeneric_font>NotGeneric</div>
      </body>
    </html>`,
  'Tests Page.setFontFamilies.');

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
  await dp.Page.setFontFamilies({fontFamilies: {
    standard: "Courier",
    sansSerif: "Ahem",
    fixed: "Ahem", // Corresponds to the "monospace" font-family.
    monospace: "Times", // Ignored, not a valid property of FontFamilies.
  }});

  // Force re-layout to make sure the font list is up-to-date.
  session.evaluate('document.body.offsetTop;');

  // Log the custom Ahem font inherited from the body.
  await logPlatformFonts('.inherited_font');

  // Log overridden generic fonts.
  await logPlatformFonts('.standard_font');
  await logPlatformFonts('.sans_serif_font');
  await logPlatformFonts('.fixed_font');

  // Log the overridden standard font as a fallback (assuming no font with name
  // "fixed" is available on the system).
  await logPlatformFonts('.notgeneric_font');

  testRunner.completeTest();
})
