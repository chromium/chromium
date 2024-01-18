(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <link rel='stylesheet' href='${testRunner.url('resources/view-transition.css')}'/>
    <div id='target'></div>
    `, 'The test verifies CSS.setStyleTexts for view-transition pseudos.');

  await session.evaluateAsync(`
     new Promise( async (resolve) => {
       // Wait for the promise below and to ensure all pseudo-elements are
       // generated before using the devtools API.
       await document.startViewTransition().ready;
       resolve();
     });
  `);

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  dp.DOM.enable();
  dp.CSS.enable();

  var event = await dp.CSS.onceStyleSheetAdded();
  var styleSheetId = event.params.header.styleSheetId;

  testRunner.log('==== Initial opacity ====');
  testRunner.log(await session.evaluate(
    `window.getComputedStyle(
        document.documentElement,
        '::view-transition-group(target)').opacity`
  ));

  // Modify the opacity property of the ::view-transition-group(target) rule.
  await cssHelper.setStyleTexts(styleSheetId, /*expectError=*/false, [
    {
      styleSheetId: styleSheetId,
      range: { startLine: 12, startColumn: 33, endLine: 14, endColumn: 0 },
      text: "\n  opacity: 0.75;\n",
    },
  ]);

  testRunner.log('==== Modified: expect 0.75 ====');
  testRunner.log(await session.evaluate(
    `window.getComputedStyle(
        document.documentElement,
        '::view-transition-group(target)').opacity`
  ));

  testRunner.completeTest();
})
