(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>

#anchor {
    position: relative;
    anchor-name: --anchor;
}

#anchored-element {
    position: absolute;
    position-fallback: --top-first;
}

@position-fallback --top-first {
    @try {
        top: anchor(--anchor top);
    }

    @try {
        top: anchor(--anchor bottom);
    }
}

</style>
<div id='anchor'>
    <div id='anchored-element'></div>
</div>
`, 'Test that position-fallback rules are reported.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var nodeId = await cssHelper.requestNodeId(documentNodeId, '#anchored-element');
  await cssHelper.loadAndDumpCSSPositionFallbacksForNode(nodeId);
  testRunner.completeTest();
});
