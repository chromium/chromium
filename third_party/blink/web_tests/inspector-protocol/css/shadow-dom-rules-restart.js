(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session} = await testRunner.startHTML(`
    <div id="host"></div>
    <template id="tmpl">
        <style> .red { color: red; } </style>
        <div id="inner" class="red">hi!</div>
    </template>
`, 'Test that style sheets hosted inside shadow roots could be inspected if inspector is reopened.');

  testRunner.log('\nDevTools session #1:');
  session.evaluate(`
    var template = document.querySelector('#tmpl');
    var root = document.querySelector('#host').attachShadow({mode: 'open'});
    root.appendChild(template.content.cloneNode(true));
  `);

  testRunner.log('\nDisconnecting devtools.');
  await session.disconnect();

  testRunner.log('\nDevTools session #2:');
  session = await page.createSession();
  var dp = session.protocol;

  await dp.DOM.enable();
  await dp.CSS.enable();

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  testRunner.log('Dumping #inner node styles in shadow root:');
  var {result} = await dp.DOM.getFlattenedDocument({pierce: true});
  var shadowRoot = result.nodes.filter(node => node.shadowRoots)[0].shadowRoots[0];
  var nodeId = await cssHelper.requestNodeId(shadowRoot.nodeId, '#inner');
  await cssHelper.loadAndDumpMatchingRulesForNode(nodeId);
  testRunner.completeTest();
})
