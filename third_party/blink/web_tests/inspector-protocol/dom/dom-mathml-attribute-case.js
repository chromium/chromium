(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <math definitionURL="x" id="main"></math>
  `, 'Test that attribute case is preserved when modifying MathML elements.');

  let response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  response = await dp.DOM.querySelector({nodeId: rootNodeId, selector: '#main'})
  const nodeId = response.result.nodeId;
  testRunner.log('Original attributes:');
  response = await dp.DOM.getAttributes({nodeId});
  for (let i = 0; i < response.result.attributes.length; i += 2)
    testRunner.log(response.result.attributes[i] + '=' + response.result.attributes[i + 1]);
  testRunner.log('Original outerHTML:');
  response = await dp.DOM.getOuterHTML({nodeId});
  testRunner.log(response.result.outerHTML);

  dp.DOM.setAttributesAsText({nodeId, name: 'definitionURL', text: 'definitionurl=y'});
  response = await dp.DOM.onceAttributeModified();
  testRunner.log('Modified attribute:');
  testRunner.log(response.params.name + '=' + response.params.value);
  testRunner.log('Modified outerHTML:');
  response = await dp.DOM.getOuterHTML({nodeId});
  testRunner.log(response.result.outerHTML);
  testRunner.completeTest();
});
