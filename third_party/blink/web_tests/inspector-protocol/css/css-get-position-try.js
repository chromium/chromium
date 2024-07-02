(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
<style>

#anchor {
    position: relative;
    anchor-name: --anchor;
}

#anchored-element {
    position: absolute;
    position-try-options: --top, --bottom;
}

@position-try --top {
  top: anchor(--anchor top);
}

@position-try --bottom {
  top: anchor(--anchor bottom);
}

</style>
<div id='anchor'>
    <div id='anchored-element'></div>
</div>
`, 'Test that position-try rules are reported.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#anchored-element');
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);
  testRunner.completeTest();
});
