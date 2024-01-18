(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='host'></div>
    <template id='tmpl'>
      <style> .red { color: red; } </style>
      <div id='inner' class='red'>hi!</div>
    </template>
  `, 'This test checks that style sheets hosted inside shadow roots are reported in CSS.styleSheetAdded event.');
  await session.evaluate(`
    var template = document.querySelector('#tmpl');
    var root = document.querySelector('#host').attachShadow({mode: 'open'});
    root.appendChild(template.content.cloneNode(true));
  `);
  dp.DOM.enable();
  dp.CSS.enable();
  var msg = await dp.CSS.onceStyleSheetAdded();
  var styleSheetHeader = msg.params.header;
  var payload = await dp.CSS.getStyleSheetText({styleSheetId: styleSheetHeader.styleSheetId});
  testRunner.log('Loaded style sheet text: ' + payload.result.text);
  testRunner.completeTest();
})
