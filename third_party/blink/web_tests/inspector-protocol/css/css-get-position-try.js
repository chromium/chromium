(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
<style>
#containing-block {
  position: relative;
  width: 200px;
  height: 200px;
}

#anchor {
  anchor-name: --anchor;
}

#anchored-element {
  width: 300px;
  height: 300px;
  position: absolute;
  position-try-fallbacks: --top, --bottom;
}

@position-try --top {
  width: 100px;
  height: 100px;
  top: anchor(--anchor top);
}

@position-try --bottom {
  width: 50px;
  height: 50px;
  top: anchor(--anchor bottom);
}

</style>
<script>
  function updateContainingBlockSize(width, height) {
    const cb = document.getElementById('containing-block');
    cb.style.width = width;
    cb.style.height = height;
  }

  function getAnchoredElementSize() {
    const el = document.getElementById('anchored-element');
    return 'w: ' + el.offsetWidth + ', h: ' + el.offsetHeight;
  }
</script>
<div id='containing-block'>
  <div id="anchor"></div>
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
  testRunner.log(await session.evaluate(`getAnchoredElementSize()`));

  // Now we reduce the containing block's size to make the first option invalid,
  // thus falling back to the second option.
  await session.evaluate(`updateContainingBlockSize('80px', '80px')`);
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);
  testRunner.log(await session.evaluate(`getAnchoredElementSize()`));

  testRunner.completeTest();
});
