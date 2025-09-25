(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {dp} = await testRunner.startHTML(`
<style>
  #host {
    color: yellow;
  }
  ::part(exposed) {
    color:red;
  }
</style>
<div id=host>
  <template shadowrootmode=open>
    <style>
      #contained {
        color: green;
      }
    </style>
    <div id=contained style="color:blue" part=exposed>Hello</div>
  </template>
</div>
`, 'Tests that matched rules have originTreeScopeNodeId set correctly for matched rules.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNode = (await dp.DOM.getDocument({})).result.root;
  const hostId = await cssHelper.requestNodeId(documentNode.nodeId, '#host');
  const shadowRoot = (await dp.DOM.describeNode({'nodeId' : hostId})).result.node.shadowRoots[0];
  const innerDivNodeId = (await dp.DOM.querySelector({nodeId: shadowRoot.nodeId, selector: '#contained'})).result.nodeId;

  const matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: innerDivNodeId});
  for (const ruleMatch of matchedStyles.result.matchedCSSRules) {
    const rule = ruleMatch.rule;
    if (rule.originTreeScopeNodeId) {
      testRunner.log(`Rule with selector '${rule.selectorList.text}' has originTreeScopeNodeId.`);
      if (rule.originTreeScopeNodeId === shadowRoot.backendNodeId) {
        testRunner.log('Its originTreeScopeNodeId matches shadow root node id.');
      } else if (rule.originTreeScopeNodeId === documentNode.backendNodeId) {
        testRunner.log('Its originTreeScopeNodeId matches document node id.');
      }
    }
  }

  testRunner.completeTest();
});
