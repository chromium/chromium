(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <div id="target" style="width: 180px; height: 280px; background: red"></div>
  `, 'Verifies that overlay is correctly rendered with emulation scale > 1.');

  await dp.DOM.enable();
  await dp.Emulation.enable();
  await dp.Overlay.enable();

  await dp.Emulation.setDeviceMetricsOverride({
    width: 200,
    height: 400,
    // Keep this the same as 'scale', because when capturing the screenshot,
    // this scale will replace 'scale'.
    deviceScaleFactor: 1.5,
    mobile: true,
    scale: 1.5,
    screenWidth: 200,
    screenHeight: 400,
  });

  const root = (await dp.DOM.getDocument()).result.root;
  const nodeId = (await dp.DOM.querySelector(
      {nodeId: root.nodeId, selector: '#target'})).result.nodeId;
  const result = await dp.Overlay.highlightNode({
    highlightConfig: {contentColor: {r: 0, g: 128, b: 0, a: 0.5}},
    nodeId: nodeId,
  });

  // Wait for overlay rendering to finish by requesting an animation frame.
  await session.evaluate(() => {
    return new Promise(resolve => requestAnimationFrame(resolve));
  });

  const imageData = (await dp.Page.captureScreenshot()).result.data;
  testRunner.log("The test passes if the image URL below is 300x600 image " +
      "containing a 270x420 brown rectangle, without any green or red.\n" +
      `data:image/png;base64,${imageData}`);

  testRunner.completeTest();
});
