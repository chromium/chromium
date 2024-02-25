(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id="host"> <h1 id="heading">headline</h1> </div>
    <script>
      const host = document.querySelector('#host');
      const shadow = host.attachShadow({mode: 'open'});
      const slot = document.createElement('slot');
      shadow.appendChild(slot);
    </script>
  `, 'Tests DOM.desctibeNode method.');
  dp.Runtime.enable();
  var {result} = await dp.Runtime.evaluate({expression: `document.getElementById('heading')`});
  var {result} = await dp.DOM.describeNode({objectId: result.result.objectId});
  testRunner.log(result.node);
  testRunner.completeTest();
})
