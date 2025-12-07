(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
<style>
#size-container {
  container-type: inline-size;
  width: 400px;
}
#scroller {
  overflow: scroll;
  container-type: inline-size;
  width: 200px;
  height: 200px;
  scroll-marker-group: before;
}
@container (width = 400px) {
  #scroller::scroll-marker-group { --foo: bar; }
}
</style>
<div id="size-container">
  <div id="scroller"></div>
</div>
`, 'Verify that @container rules reported with the correct values.');
  await dp.DOM.enable();
  await dp.CSS.enable();
  const document = await dp.DOM.getDocument();
  const sizeContainer = await dp.DOM.querySelector({
    nodeId: document.result.root.nodeId,
    selector: '#size-container',
  });

  const setChildNodesResponses = [];
  dp.DOM.onSetChildNodes((message) => setChildNodesResponses.push(message));
  await dp.DOM.requestChildNodes({nodeId: sizeContainer.result.nodeId});

  const scroller = setChildNodesResponses[0].params.nodes[0];
  const styles = await dp.CSS.getMatchedStylesForNode({nodeId: scroller.pseudoElements[0].nodeId});
  testRunner.log(styles.result.matchedCSSRules, "Dumping styles for " + scroller.pseudoElements[0].localName + ": ");
  testRunner.completeTest();
});
