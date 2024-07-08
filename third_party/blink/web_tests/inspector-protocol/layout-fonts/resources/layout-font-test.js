(async function layoutFontTest(testRunner, session) {
  await session.evaluateAsync("document.fonts.ready");
  var documentNodeId = (await session.protocol.DOM.getDocument()).result.root.nodeId;
  await session.protocol.CSS.enable();
  var testNodes = await session.evaluate(`
    Array.prototype.map.call(document.querySelectorAll('.test div'), e => ({selector: '#' + e.id, textContent: e.textContent}))
  `);

  for (var testNode of testNodes) {
    var nodeId = (await session.protocol.DOM.querySelector({nodeId: documentNodeId , selector: testNode.selector})).result.nodeId;
    var response = await session.protocol.CSS.getPlatformFontsForNode({nodeId});
    var usedFonts = response.result.fonts;
    usedFonts.sort((a, b) => b.glyphCount - a.glyphCount || a.familyName.localeCompare(b.familyName));

    testRunner.log(testNode.textContent.trim());
    testRunner.log(testNode.selector + ':');
    testRunner.log(usedFonts.map(usedFont => `"${usedFont.familyName}" : ${usedFont.glyphCount}`).join(',\n') + '\n');
    testNode.usedFonts = usedFonts;
  }
  return testNodes;
})
