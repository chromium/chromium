(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <main>
    <article>
    <div id="hidden" style="display: none">hidden div</div>
    </article>
    </main>
  `, 'Tests that fetching the tree for a node without an AXNode functions correctly.');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('#hidden', true, msg);
})
