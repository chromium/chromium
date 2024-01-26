(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <style>
      #element {
        width: 400px;
        height: 500px;
      }
    </style>
    <div id="element"></div>
  `, 'Verifies that Overlay.setShowIsolatedElements works.');

  await dp.DOM.enable();
  await dp.Overlay.enable();

  const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
  const {result: {nodeId}} = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#element'});

  async function waitForAnimationFrame() {
    await session.evaluateAsync(() => {
      return new Promise(resolve => requestAnimationFrame(resolve));
    });
  }

  async function getDrawIsolatedElementHighlightCommands() {
    await waitForAnimationFrame();
    return await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        // Multiple frames could be rendered but they should to be identical.
        const commands = window.commands;
        window.commands = [];
        return JSON.stringify(commands.filter(command => command.name === 'drawIsolatedElementHighlight').shift(), null, 2);
      })()`);
    });
  }

  await dp.Overlay.setInspectMode({
    mode: 'searchForNode',
    highlightConfig: {},
  });

  await session.evaluate(() => {
    return internals.evaluateInInspectorOverlay(`(function () {
      window.commands = [];
      window.dispatch = ([name, data]) => {
        window.commands.push({name, data});
      };
    })()`);
  });

  testRunner.log('Overlay not rendered:');
  testRunner.log(await getDrawIsolatedElementHighlightCommands());

  const black = {
    r: 0,
    g: 0,
    b: 0,
    a: 1,
  };
  await dp.Overlay.setShowIsolatedElements({
    isolatedElementHighlightConfigs: [{
      nodeId,
      isolationModeHighlightConfig: {
        resizerColor: black,
        resizerHandleColor: black,
        maskColor: black,
      },
    }]
  });

  testRunner.log('Overlay rendered:');
  testRunner.log(await getDrawIsolatedElementHighlightCommands());

  testRunner.completeTest();
});
