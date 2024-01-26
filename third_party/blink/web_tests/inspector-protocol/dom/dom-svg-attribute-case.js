(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <svg id='main' xmlns='http://www.w3.org/2000/svg' width='600' height='500' viewBox='0 0 100 120' />
  `, 'Test that DOM attribute case is preserved when modifying SVG elements.');

  let response = await dp.DOM.getDocument();
  const rootNodeId = response.result.root.nodeId;

  response = await dp.DOM.querySelector({nodeId: rootNodeId, selector: '#main'})
  const nodeId = response.result.nodeId;
  testRunner.log('Original attributes:');
  response = await dp.DOM.getAttributes({nodeId});
  for (let i = 0; i < response.result.attributes.length; i += 2)
    testRunner.log(response.result.attributes[i] + '=' + response.result.attributes[i + 1]);

  dp.DOM.setAttributesAsText({nodeId, name: 'viewBox', text: 'viewBox="0 0 120 120"'});
  response = await dp.DOM.onceAttributeModified();
  testRunner.log('Modified attribute:');
  testRunner.log(response.params.name + '=' + response.params.value);
  testRunner.completeTest();
});
