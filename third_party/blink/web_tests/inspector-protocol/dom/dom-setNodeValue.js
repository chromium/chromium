(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <div id="x"></div>
    <script>
      x.append('foo');
      x.append('bar');
      x.append('baz');
    </script>
  `, 'Tests how DOM domain works with setNodeValue.');

  const documentMessage = await dp.DOM.getDocument();
  const textNodesContainerMessage = await dp.DOM.querySelector({ nodeId: documentMessage.result.root.nodeId, selector: "#x" });
  dp.DOM.requestChildNodes({ nodeId: textNodesContainerMessage.result.nodeId });
  const eventForChildNodesWithFoo = await dp.DOM.onceSetChildNodes(event => !!event.params.nodes.find(node => node.nodeValue === "foo"));
  const fooNode = eventForChildNodesWithFoo.params.nodes.find(node => node.nodeValue === "foo");
  testRunner.log("\nBefore setNodeValue for foo:")
  testRunner.log((await dp.DOM.getOuterHTML({nodeId: textNodesContainerMessage.result.nodeId})).result.outerHTML);

  await dp.DOM.setNodeValue({ nodeId: fooNode.nodeId, value: "notanymore" });

  testRunner.log("\nAfter setNodeValue for foo:")
  testRunner.log((await dp.DOM.getOuterHTML({nodeId: textNodesContainerMessage.result.nodeId})).result.outerHTML);
  testRunner.completeTest();
})
