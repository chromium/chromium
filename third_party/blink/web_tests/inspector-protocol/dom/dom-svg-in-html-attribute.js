(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <svg></svg>
  `, 'Test that SVG-in-HTML attribute modifications are treated as HTML, not XML');

  let response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  response = await dp.DOM.querySelector({nodeId: rootNodeId, selector: 'svg'});
  const nodeId = response.result.nodeId;

  testRunner.log('Original outerHTML:');
  response = await dp.DOM.getOuterHTML({nodeId});
  testRunner.log(response.result.outerHTML);

  dp.DOM.setAttributesAsText({nodeId, name: 'xlink:href', text: 'xlink:href=""'});
  response = await dp.DOM.onceAttributeModified();
  testRunner.log('Modified attribute:');
  testRunner.log(response.params.name + '=' + JSON.stringify(response.params.value));

  testRunner.log('Modified outerHTML:');
  response = await dp.DOM.getOuterHTML({nodeId});
  testRunner.log(response.result.outerHTML);

  testRunner.completeTest();
});
