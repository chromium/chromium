(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(``,
    "Verifies that overlay of an iframe element doesn't crash.");

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
    `;
  });

  await dp.DOM.enable();
  await dp.Emulation.enable();
  await dp.Overlay.enable();

  const root = (await dp.DOM.getDocument()).result.root;
  const iframeDiv = (await dp.DOM.getNodeForLocation({x: 250, y: 250})).result.nodeId;

  const result = await dp.Overlay.highlightNode({
    highlightConfig: {contentColor: {r: 0, g: 128, b: 0, a: 0.5}},
    nodeId: iframeDiv,
  });

  // Wait for overlay rendering to finish by requesting an animation frame.
  await session.evaluate(() => {
    return new Promise(resolve => requestAnimationFrame(resolve));
  });

  testRunner.completeTest();
});
