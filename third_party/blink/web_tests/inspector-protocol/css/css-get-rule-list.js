(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/coverage.css')}'>
</head>
<h1 class='class'>Class Selector</h1>
<p id='id'>ID Selector</p>
<div></div>`, 'Test rule usage tracking');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);
  await dp.DOM.enable();
  await dp.CSS.enable();

  await dp.CSS.startRuleUsageTracking();
  var rules = (await dp.CSS.stopRuleUsageTracking()).result.ruleUsage;
  rules.sort((a, b) => a.startOffset - b.startOffset);
  var usedLines = rules.filter(rule => rule.used);
  var unusedLines = rules.filter(rule => !rule.used);

  testRunner.log('Used rules offsets: ' + usedLines.length);
  for (var line of usedLines)
      testRunner.log(line.startOffset);

  testRunner.log('Unused rules offsets: ' + unusedLines.length);
  for (var line of unusedLines)
      testRunner.log(line.startOffset);

  testRunner.completeTest();
})
