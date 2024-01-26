(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
/** This style should be inherited by shadow DOM */
body {
    color: blue;
}
</style>
<template>
    <div id='inspected'>Inspect me.</div>
</template>
<div id='shadow-host'></div>
`, 'Tests that shadow DOM styles inherit styles properly.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();
  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var nodeId = await cssHelper.requestNodeId(documentNodeId, '#shadow-host');

  session.evaluate(() => {
    var shadowRoot = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('template');
    var clone = document.importNode(template.content, true);
    shadowRoot.appendChild(clone);
  });

  var event = await dp.DOM.onceShadowRootPushed();
  var nodeId = await cssHelper.requestNodeId(event.params.root.nodeId, '#inspected');
  await cssHelper.loadAndDumpMatchingRulesForNode(nodeId);
  testRunner.completeTest();
});
