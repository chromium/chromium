(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
<style>
  a {
    color: red;
  }
  a:visited {
    color: green;
  }
  a:link {
    color: blue;
  }
</style>
<div>
  This link is <a id="unvisited" href="https://something.something/xxx">unvisited</a>.
  This link is <a id="visited" href="">visited</a>.
</div>`,
      'Test CSS.forcePseudoStates method for :link and :visited');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  const unvisitedNodeId =
      await cssHelper.requestNodeId(documentNodeId, '#unvisited');
  const visitedNodeId =
      await cssHelper.requestNodeId(documentNodeId, '#visited');

  async function logMatchedRules(nodeId) {
    const result = await dp.CSS.getMatchedStylesForNode({nodeId});
    for (const rule of result.result.matchedCSSRules) {
      if (rule.rule.origin !== 'user-agent') {
        testRunner.log(`selector: ${rule.rule.selectorList.selectors[0].text}`);
      }
    }
  }

  testRunner.log('Matched rules for #unvisited:');
  await logMatchedRules(unvisitedNodeId);
  testRunner.log('Matched rules for #visited:');
  await logMatchedRules(visitedNodeId);
  testRunner.log('Forcing new pseudo states');
  await dp.CSS.forcePseudoState(
      {nodeId: unvisitedNodeId, forcedPseudoClasses: ['visited']});
  await dp.CSS.forcePseudoState(
      {nodeId: visitedNodeId, forcedPseudoClasses: ['link']});
  testRunner.log('Matched rules for #unvisited:');
  await logMatchedRules(unvisitedNodeId);
  testRunner.log('Matched rules for #visited:');
  await logMatchedRules(visitedNodeId);

  await dp.CSS.disable();
  await dp.DOM.disable();

  testRunner.log('Completed');

  testRunner.completeTest();
});
