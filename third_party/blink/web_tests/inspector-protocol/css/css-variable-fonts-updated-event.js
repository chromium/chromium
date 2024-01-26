(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
    `some text`,
    'Verifies that CSS.fontsUpdated events contain additional information for variable fonts'
  );

  await dp.DOM.enable();
  await dp.CSS.enable();
  session.evaluate(fontURL => {
    const style = document.createElement('style');
    style.innerText = `
      @font-face {
        font-family: "Sixtyfour";
        src: url('../../third_party/Homecomputer/Sixtyfour.ttf');
      }
      body {
        font-family: "Sixtyfour";
      }
    `;
    document.head.appendChild(style);
  });
  event = await dp.CSS.onceFontsUpdated(
                       event => typeof event.params.font !== 'undefined' &&
                       event.params.font.fontFamily === 'Sixtyfour');

  const font = event.params.font;
  testRunner.log('Variation axes of the Sixtyfour font (can be verified using Python fontTools or wakamaifondue.com):');
  testRunner.log(font.fontVariationAxes);
  testRunner.completeTest();
});
