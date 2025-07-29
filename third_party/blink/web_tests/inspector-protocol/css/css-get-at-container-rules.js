(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
@container not anchored(fallback) {
  #anchored-child { --anchored: true }
}
@container not (scroll-state(scrollable) and (inline-size > 10000px)) {
  #scroll-state-child { --scroll-state: true }
}
#anchored { container-type: anchored; }
#scroll-state { container-type: inline-size scroll-state; }
</style>
<div id="anchored">
  <div id="anchored-child"></div>
</div>
<div id="scroll-state">
  <div id="scroll-state-child"></div>
</div>
`, 'Verify that nested @container rules reported with the correct values.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;
  const anchoredChild = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: '#anchored-child',
  });
  const anchoredChildId = anchoredChild.result.nodeId;
  let matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: anchoredChildId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    if (ruleMatch.rule.ruleTypes.length > 0) {
      testRunner.log(ruleMatch.rule.containerQueries);
    }
  }

  const scrollStateChild = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: '#scroll-state-child',
  });
  const scrollStateChildId = scrollStateChild.result.nodeId;
  matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: scrollStateChildId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    if (ruleMatch.rule.ruleTypes.length > 0) {
      testRunner.log(ruleMatch.rule.containerQueries);
    }
  }

  testRunner.completeTest();
});
