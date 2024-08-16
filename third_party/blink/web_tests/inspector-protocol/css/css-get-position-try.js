(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
<style>
#containing-block {
  position: relative;
  width: 200px;
  height: 200px;
}

#anchor {
  position: relative;
  anchor-name: --anchor;
}

#anchored-element {
  width: 300px;
  height: 300px;
  position: absolute;
  position-anchor: --anchor;
  position-try-fallbacks: flip-block, --top, --bottom;
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

  function fallbackToFlipBlock() {
    updateContainingBlockSize('400px', '400px');
    const anchor = document.getElementById('anchor');
    anchor.style.top = '100px';
    anchor.style.left = '100px';
    anchor.style.width = '100px';
    anchor.style.height = '100px';
    const anchoredElement = document.getElementById('anchored-element');
    anchoredElement.style.width = '100px';
    anchoredElement.style.height = '200px';
    anchoredElement.style.positionArea = 'top center';
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
  testRunner.log(await session.evaluate(`getAnchoredElementSize()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  // Now we reduce the containing block's size to make the first option invalid,
  // thus falling back to the second option.
  await session.evaluate(`updateContainingBlockSize('80px', '80px')`);
  testRunner.log(await session.evaluate(`getAnchoredElementSize()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  // Now we increase the containing block's size to make the default styles work.
  await session.evaluate(`updateContainingBlockSize('500px', '500px')`);
  testRunner.log(await session.evaluate(`getAnchoredElementSize()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  // Now we update styles to make the first keyword fallback work.
  await session.evaluate(`fallbackToFlipBlock()`);
  testRunner.log(await session.evaluate(`getAnchoredElementSize()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  testRunner.completeTest();
});
