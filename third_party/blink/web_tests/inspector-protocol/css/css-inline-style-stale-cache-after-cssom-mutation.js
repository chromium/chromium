(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // Regression test:
  //   1. Element with populated inline style.
  //   2. First getInlineStylesForNode → seeds InspectorStyle source-data cache.
  //   3. CSSOM removeProperty (fires probe::DidInvalidateStyleAttr only).
  //   4. Second getInlineStylesForNode → must reflect post-mutation state.
  // Pre-fix the cached source-data ranges outlived the now-shorter attribute
  // text and the second build crashed on InspectorStyle::TextForRange.
  const {session, dp} = await testRunner.startHTML(
      `<div id='inspected' style='color: red; padding: 100px'></div>`,
      'Inline style cache must be invalidated after a CSSOM mutation');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);
  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');

  function propertyNames(style) {
    return style.cssProperties.filter(p => p.range).map(p => p.name).sort();
  }

  // 1. First build seeds the cache.
  const first =
      (await dp.CSS.getInlineStylesForNode({nodeId})).result.inlineStyle;
  testRunner.log('first properties: ' + propertyNames(first).join(', '));

  // 2. Issue the CSSOM mutation; the next protocol request is serialized after
  // it.
  session.evaluate(
      () =>
          document.getElementById('inspected').style.removeProperty('padding'));
  const second = await dp.CSS.getInlineStylesForNode({nodeId});
  testRunner.log(
      'second properties: ' +
      propertyNames(second.result.inlineStyle).join(', '));

  testRunner.completeTest();
})
