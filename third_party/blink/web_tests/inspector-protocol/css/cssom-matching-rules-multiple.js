(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
#test {
    color: red;
}

#test {
    color: green;
}

#test {
    color: blue;
}

#test {
    width: 10%;
}

#test {
    width: 20%;
}

#test {
    width: 30%;
}

#test {
    width: 40%;
}

#test {
    width: 50%;
}

#test {
    width: 60%;
}
</style>
<article id='test'></article>
`, 'The test verifies CSS.getMatchedStylesForNode when used concurrently with multiple CSSOM modifications.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.log('Running test: testModifyRule');
  testRunner.log('--------------');
  testRunner.log('Original rule:');
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test', true);

  testRunner.log('Mutating 3rd:');
  testRunner.log('---------------');
  session.evaluate(() => document.styleSheets[0].rules[3].style.setProperty('color', 'red'));
  session.evaluate(() => document.styleSheets[0].rules[3].style.removeProperty('width'));
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test', true);

  testRunner.log('Mutating 4th:');
  testRunner.log('--------------');
  session.evaluate(() => document.styleSheets[0].rules[4].style.setProperty('color', 'green'));
  session.evaluate(() => document.styleSheets[0].rules[4].style.removeProperty('width'));
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test', true);

  testRunner.log('Mutating 5th:');
  testRunner.log('--------------');
  session.evaluate(() => document.styleSheets[0].rules[5].style.setProperty('color', 'blue'));
  session.evaluate(() => document.styleSheets[0].rules[5].style.removeProperty('width'));
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test', true);

  testRunner.log('Delete first 3:');
  testRunner.log('---------------');
  session.evaluate(() => {
    for (var i = 0; i < 3; ++i)
      document.styleSheets[0].removeRule(0)
  });
  await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#test', true);

  testRunner.completeTest();
})
