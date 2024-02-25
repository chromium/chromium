(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel="stylesheet" type="text/css" href="${testRunner.url("resources/cssom-matching-rules-modify-rule.css")}">
<link rel="stylesheet" type="text/css" href="${testRunner.url("resources/cssom-matching-rules-insert-rule.css")}">
<link rel="stylesheet" type="text/css" href="${testRunner.url("resources/cssom-matching-rules-remove-rule.css")}">
<link rel="stylesheet" type="text/css" href="${testRunner.url("resources/cssom-matching-rules-matching-prefix.css")}">
<link rel="stylesheet" type="text/css" href="${testRunner.url("resources/cssom-matching-rules-matching-suffix.css")}">

<article id='modifyRule'></article>
<article id='insertRule'></article>
<article id='removeRule'></article>
<article id='matchingPrefix'></article>
<article id='matchingSuffix'></article>`, 'The test verifies CSS.getMatchedStylesForNode when used concurrently with the CSSOM modifications for external stylesheets.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.runTestSuite([
    async function testModifyRule() {
      testRunner.log('--------------');
      testRunner.log('Original rule:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#modifyRule', true);

      session.evaluate(() => document.styleSheets[0].rules[0].style.setProperty('color', 'red'));
      testRunner.log('--------------');
      testRunner.log('Modified rule 1:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#modifyRule', true);

      session.evaluate(() => document.styleSheets[0].rules[2].style.setProperty('color', 'blue'));
      testRunner.log('---------------');
      testRunner.log('Modified rule 3:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#modifyRule', true);

      session.evaluate(() => document.styleSheets[0].rules[1].style.setProperty('color', 'green'));
      testRunner.log('---------------');
      testRunner.log('Modified rule 2:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#modifyRule', true);

      session.evaluate(() => document.styleSheets[0].rules[1].style.removeProperty('color'));
      testRunner.log('---------------');
      testRunner.log('Restored rule 2:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#modifyRule', true);

      session.evaluate(() => document.styleSheets[0].rules[0].style.removeProperty('color'));
      session.evaluate(() => document.styleSheets[0].rules[2].style.removeProperty('color'));
      testRunner.log('-----------------');
      testRunner.log('Restored rule 1,3:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#modifyRule', true);
    },

    async function testInsertFirstRule() {
      await testInsertRule(0);
    },

    async function testInsertMiddleRule() {
      await testInsertRule(1);
    },

    async function testInsertLastRule() {
      await testInsertRule(2);
    },

    async function testRemoveRule() {
      testRunner.log('Original rule:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#removeRule', true);

      session.evaluate(() => document.styleSheets[2].removeRule(0));
      testRunner.log('-------------------');
      testRunner.log('After remove rule 1:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#removeRule', true);

      session.evaluate(() => document.styleSheets[2].removeRule(0));
      testRunner.log('-------------------');
      testRunner.log('After remove rule 2:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#removeRule', true);
    },

    async function testMatchingPrefix() {
      testRunner.log('Original rule:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#matchingPrefix', true);

      session.evaluate(() => document.styleSheets[3].cssRules[2].style.setProperty('color', 'purple'));
      testRunner.log('--------------');
      testRunner.log('Modified rule 3:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#matchingPrefix', true);
    },

    async function testMatchingSuffix() {
      testRunner.log('Original rule:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#matchingSuffix', true);

      session.evaluate(() => document.styleSheets[4].cssRules[0].style.setProperty('color', 'purple'));
      testRunner.log('--------------');
      testRunner.log('Modified rule 0:');
      await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#matchingSuffix', true);
    }
  ]);

  async function testInsertRule(index) {
    testRunner.log('Original rule:');
    await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#insertRule', true);

    session.evaluate(index => document.styleSheets[1].insertRule('#insertRule { color: red }', index), index);
    testRunner.log('--------------');
    testRunner.log('After inserted rule:');
    await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#insertRule', true);

    session.evaluate(index => document.styleSheets[1].removeRule(index), index);
    testRunner.log('--------------');
    testRunner.log('Restored rule:');
    await cssHelper.loadAndDumpMatchingRules(documentNodeId, '#insertRule', true);
  }
})

