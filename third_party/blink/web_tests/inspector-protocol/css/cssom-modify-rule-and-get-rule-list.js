(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
#modifyRule {
    box-sizing: border-box;
}

#modifyRule {
    height: 100%;
}

#modifyRule {
    width: 100%;
}
</style>
<article id='modifyRule'></article>
`, 'The test verifies that CSS.stopRuleUsageTracking doesn\'t crash when used concurrently with the CSSOM modifications.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();
  var documentNodeId = await cssHelper.requestDocumentNodeId();

  await dp.CSS.startRuleUsageTracking();

  session.evaluate(`document.styleSheets[0].rules[0].style.setProperty('color', 'red')`);
  session.evaluate(`document.styleSheets[0].rules[2].style.setProperty('color', 'blue')`);
  session.evaluate(`document.styleSheets[0].rules[1].style.setProperty('color', 'green')`);

  var response = await dp.CSS.stopRuleUsageTracking();

  var rules = response.result.ruleUsage;
  var usedLines = rules.filter(rule => rule.used);
  var unusedLines = rules.filter(rule => !rule.used);

  usedLines.sort();
  unusedLines.sort();
  testRunner.log('=== Size of array: ' + rules.length);
  testRunner.log('   Number of used Rules: ' + usedLines.length);
  for(var line of usedLines)
      testRunner.log(line.range.startLine);

  testRunner.log('   Number of unused Rules: ' + unusedLines.length);
  for(var line of unusedLines)
      testRunner.log(line.range.startLine);

  testRunner.completeTest();
})
