(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    Several<br>
    Lines<br>
    Of<br>
    Text<br>
    <div style='position:absolute;top:100;left:0;width:100;height:100;background:red'></div>
    <div style='position:absolute;top:200;left:100;width:100;height:100;background:green'></div>
    <div style='position:absolute;top:150;left:50;width:100;height:100;background:blue;transform:rotate(45deg);'></div>
  `, 'Tests DOM.getBoxModel method.');

  await session.evaluate(() => {
    var iframe = document.createElement('iframe');
    iframe.style.position = 'absolute';
    iframe.style.top = '200px';
    iframe.style.left = '200px';
    iframe.style.width = '500px';
    iframe.style.height = '500px';
    document.body.appendChild(iframe);
    iframe.contentWindow.document.body.innerHTML = `
      <div style="width:100px;height:100px;background:orange"></div>
      <svg xmlns="http://www.w3.org/2000/svg" width="500" height="500" style="position:absolute;top:200px;left:200px;">
        <rect id="theRect" x="30" y="50" width="100" height="100"></rect>
      </svg>
    `;
  });

  var NodeTracker = await testRunner.loadScript('../resources/node-tracker.js');
  var nodeTracker = new NodeTracker(dp);
  const bn1 = (await dp.DOM.getNodeForLocation({x: 100, y: 200})).result.backendNodeId;
  const bn2 = (await dp.DOM.getNodeForLocation({x: 250, y: 250})).result.backendNodeId;
  const bn3 = (await dp.DOM.getNodeForLocation({x: 500, y: 500})).result.backendNodeId;

  await dp.DOM.enable();
  await dp.DOM.getDocument();
  await nodeTracker.nodeForBackendId(bn1);
  await nodeTracker.nodeForBackendId(bn2);
  await nodeTracker.nodeForBackendId(bn3);

  for (var nodeId of nodeTracker.nodeIds()) {
    var node = nodeTracker.nodeForId(nodeId);
    if (node.nodeName !== 'DIV' && node.nodeName !== 'rect')
      continue;

    await dp.Emulation.clearDeviceMetricsOverride();
    var message = await dp.DOM.getBoxModel({nodeId});

    await dp.Emulation.setDeviceMetricsOverride({
      width: 800,
      height: 600,
      mobile: true,
      deviceScaleFactor: 2
    });
    var emulatedMessage = await dp.DOM.getBoxModel({nodeId});
    if (message.error)
      testRunner.log(node.nodeName + ': ' + message.error.message);
    else if (emulatedMessage.error)
      testRunner.log(node.nodeName + ': ' + message.error.message);
    else if (!quadsMatch(message.result.model.content, emulatedMessage.result.model.content))
      testRunner.log(node.nodeName + ': content does not match emulated content.')
    else
      testRunner.log(message.result.model.content, node.nodeName + ' ' + node.attributes + ' ');
  }
  testRunner.completeTest();

  function quadsMatch(a, b) {
    for (var i = 0; i < a.length; i++) {
      if (Math.round(a[i] * 1000) !== Math.round(b[i] * 1000))
        return false;
    }
    return true;
  }
})
