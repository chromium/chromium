(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
    <div id="declarative">
      <template shadowrootmode="open">
        <div>contents</div>
      </template>
    </div>
    <div id="js">
    </div>
    <script>
      const div = document.createElement('div');
      div.textContent = 'more contents';
      document.getElementById('js').attachShadow({mode: "open"}).appendChild(div);
    </script>
  `,
      'Tests how DOM domain works with getOuterHTML.');

  const {result: {root: {nodeId: docId}}} = await dp.DOM.getDocument();
  const {result: {nodeId: declarativeId}} =
      await dp.DOM.querySelector({nodeId: docId, selector: '#declarative'});
  const {result: {nodeId: jsId}} =
      await dp.DOM.querySelector({nodeId: docId, selector: '#js'});

  testRunner.log('\ngetOuterHTML(declarativeId, includeShadowDOM=false):');
  testRunner.log(await dp.DOM.getOuterHTML(
      {nodeId: declarativeId, includeShadowDOM: false}));

  testRunner.log('\ngetOuterHTML(declarativeId, includeShadowDOM=true):');
  testRunner.log(await dp.DOM.getOuterHTML(
      {nodeId: declarativeId, includeShadowDOM: true}));

  testRunner.log('\ngetOuterHTML(jsId, includeShadowDOM=false):');
  testRunner.log(
      await dp.DOM.getOuterHTML({nodeId: jsId, includeShadowDOM: false}));

  testRunner.log('\ngetOuterHTML(jsId, includeShadowDOM=true):');
  testRunner.log(
      await dp.DOM.getOuterHTML({nodeId: jsId, includeShadowDOM: true}));
  testRunner.completeTest();
});
