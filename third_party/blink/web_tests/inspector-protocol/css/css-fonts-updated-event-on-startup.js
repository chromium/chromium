(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(
    `<style>
      @font-face {
        font-family: Amstelvar;
        src: url('../../third_party/Amstelvar/Amstelvar.ttf');
      }
      body {
        font-family: Amstelvar;
      }
    </style>
    some text`,
    'Verifies that CSS.fontsUpdated events are sent after CSS domain is enabled'
  );
  const eventPromise = dp.CSS.onceFontsUpdated(
    event => typeof event.params.font !== 'undefined' &&
    event.params.font.fontFamily === 'Amstelvar');
  await dp.DOM.enable();
  await dp.CSS.enable();
  const event = await eventPromise;
  const font = event.params.font;
  testRunner.log(font, null, ['src'])
  testRunner.log('SUCCESS: CSS.FontsUpdated events received.');
  testRunner.completeTest();
});
