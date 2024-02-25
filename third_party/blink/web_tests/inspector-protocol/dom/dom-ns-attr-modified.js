(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <svg>
        <a id='main' xlink:href='http://localhost'>link</a>
    </svg>
  `, 'Test that DOM events have correct parameters for attribute with namespace in XML document.');

  var response = await dp.DOM.getDocument();
  await dp.DOM.querySelector({nodeId: response.result.root.nodeId, selector: '#main'});

  testRunner.log('\nChanging attribute...');
  session.evaluate(() => {
    var element = document.getElementById('main');
    element.setAttributeNS('http://www.w3.org/1999/xlink', 'xlink:href', 'changed-url');
  });
  var msg = await dp.DOM.onceAttributeModified();
  var result = msg.params;
  testRunner.log(`Modified attribute: '${result.name}'='${result.value}'`);

  testRunner.log('Removing attribute...');
  session.evaluate(() => {
    var element = document.getElementById('main');
    element.removeAttribute('xlink:href', 'changed-url');
  });
  msg = await dp.DOM.onceAttributeRemoved();
  var result = msg.params;
  testRunner.log(`Removed attribute: '${result.name}'`);
  testRunner.completeTest();
});

