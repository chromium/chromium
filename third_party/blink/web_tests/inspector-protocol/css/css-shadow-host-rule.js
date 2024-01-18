(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<template id='shadow-template'>
<style>
:host {
    color: red;
}
</style>
<div>Hi!</div>
</template>
<div id='shadow-host'></div>`, 'Tests that rules in shadow host are reported in matched styles');

  await session.evaluate(() => {
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
  });

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();
  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var nodeId = await cssHelper.requestNodeId(documentNodeId, '#shadow-host');
  await cssHelper.loadAndDumpMatchingRulesForNode(nodeId);
  testRunner.completeTest();
})
