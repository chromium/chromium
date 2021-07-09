(async function(testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <style>
      #container {
        width: 400px;
        height: 500px;
      }
    </style>
    <div id="container"></div>
  `, 'Verifies that Overlay.setShowContainerQueryOverlays works.');

  await dp.DOM.enable();
  await dp.Overlay.enable();

  const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
  const {result: {nodeId}} = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#container'});

  async function waitForAnimationFrame() {
    await session.evaluateAsync(() => {
      return new Promise(resolve => requestAnimationFrame(resolve));
    });
  }

  async function getDrawContainerQueryHighlightCommands() {
    await waitForAnimationFrame();
    return await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        // Multiple frames could be rendered but they should to be identical.
        const commands = window.commands;
        window.commands = [];
        return JSON.stringify(commands.filter(command => command.name === 'drawContainerQueryHighlight').shift(), null, 2);
      })()`);
    });
  }

  async function getHighlightNodeCommands() {
    await waitForAnimationFrame();
    return await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        // Multiple frames could be rendered but they should to be identical.
        const commands = window.commands;
        window.commands = [];
        return JSON.stringify(commands
          .filter(command => command.name === 'drawHighlight')
          .shift().data.containerQueryInfo, null, 2);
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
  testRunner.log(await getDrawContainerQueryHighlightCommands());

  const black = {
    r: 0,
    g: 0,
    b: 0,
    a: 1,
  };
  await dp.Overlay.setShowContainerQueryOverlays({
    containerQueryHighlightConfigs: [{
      nodeId,
      containerQueryContainerHighlightConfig: {
        containerBorder: {
          color: black,
        },
      },
    }]
  });

  testRunner.log('Overlay rendered:');
  testRunner.log(await getDrawContainerQueryHighlightCommands());

  await dp.Overlay.highlightNode({
    highlightConfig: {
      containerQueryContainerHighlightConfig: {
        containerBorder: {
          color: black,
          patten: 'dashed',
        },
      },
    },
    nodeId,
  });

  testRunner.log('Overlay rendered:');
  testRunner.log(await getHighlightNodeCommands());

  testRunner.completeTest();
});
