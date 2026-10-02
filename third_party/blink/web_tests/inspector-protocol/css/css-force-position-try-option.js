(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startHTML(`
<style>
#containing-block {
  position: relative;
  width: 200px;
  height: 200px;
}

#anchor {
  position: relative;
  anchor-name: --anchor;
  top: 50px;
  left: 50px;
  width: 20px;
  height: 20px;
}

#anchored-element {
  container-type: anchored;
  width: 250px;
  height: 250px;
  position: absolute;
  position-anchor: --anchor;
  position-visibility: no-overflow;
  position-try-fallbacks: flip-block, --medium, --large;
}

@position-try --medium {
  width: 100px;
  height: 100px;
  top: anchor(--anchor top);
}

@position-try --large {
  width: 300px;
  height: 300px;
  top: anchor(--anchor bottom);
}

#child {
  color: rgb(0, 0, 0);
}

@container anchored(fallback: none) {
  #child {
    color: rgb(0, 0, 255);
  }
}

@container anchored(fallback: flip-block) {
  #child {
    color: rgb(0, 255, 0);
  }
}

@container anchored(fallback: --large) {
  #child {
    color: rgb(255, 0, 0);
  }
}
</style>
<script>
  function getAnchoredElementInfo() {
    const el = document.getElementById('anchored-element');
    const child = document.getElementById('child');
    const rect = el.getBoundingClientRect();
    return 'w: ' + rect.width + ', h: ' + rect.height + ', child color: ' + getComputedStyle(child).color;
  }
</script>
<div id="containing-block">
  <div id="anchor"></div>
  <div id="anchored-element">
    <div id="child"></div>
  </div>
</div>
`,
                                 'Test CSS.forcePositionTryOption command.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId =
      await cssHelper.requestNodeId(documentNodeId, '#anchored-element');

  testRunner.log(
      '--- Initial state (base 250x250 overflows 200x200; falls back to --medium 100x100, index 1) ---');
  testRunner.log(await session.evaluate(`getAnchoredElementInfo()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  testRunner.log('\n--- Force index 0 (base style 250x250) ---');
  await dp.CSS.forcePositionTryOption({nodeId, index: 0});
  testRunner.log(await session.evaluate(`getAnchoredElementInfo()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  testRunner.log(
      '\n--- Force index 1 (flip-block 250x250, index 0 in position-try-fallbacks) ---');
  await dp.CSS.forcePositionTryOption({nodeId, index: 1});
  testRunner.log(await session.evaluate(`getAnchoredElementInfo()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  testRunner.log(
      '\n--- Force index 3 (--large 300x300, index 2 in position-try-fallbacks, overflows containing block) ---');
  await dp.CSS.forcePositionTryOption({nodeId, index: 3});
  testRunner.log(await session.evaluate(`getAnchoredElementInfo()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  testRunner.log(
      '\n--- Clear forced option (omitting index) -> restores natural fallback (--medium 100x100, index 1) ---');
  await dp.CSS.forcePositionTryOption({nodeId});
  testRunner.log(await session.evaluate(`getAnchoredElementInfo()`));
  await cssHelper.loadAndDumpCSSPositionTryForNode(nodeId);

  testRunner.completeTest();
});
