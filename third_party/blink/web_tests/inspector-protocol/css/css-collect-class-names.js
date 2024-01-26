(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('./resources/collect-class-names.css')}'/>
<style>
.inline1 {
    font-size: 12px;
}
</style>`, 'Verify CSS.collectClassNames method.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);
  dp.DOM.enable();
  dp.CSS.enable();

  var styleSheets = [];
  for (var i = 0; i < 3; ++i)
    styleSheets.push(await dp.CSS.onceStyleSheetAdded());

  var styleSheetClasses = [];
  for (var styleSheet of styleSheets) {
    var response = await dp.CSS.collectClassNames({styleSheetId: styleSheet.params.header.styleSheetId});
    styleSheetClasses.push(...response.result.classNames);
  }

  styleSheetClasses.sort();
  for (var i = 0; i < styleSheetClasses.length; i++)
    testRunner.log(styleSheetClasses[i]);
  testRunner.completeTest();
});

