(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
    <style>
      #snap {
        background-color: white;
        scroll-snap-type: y mandatory;
        overflow-x: hidden;
        overflow-y: scroll;
        width: 150px;
        height: 150px;
      }
      #snap > div {
        width: 75px;
        height: 75px;
        margin: 10px;
        padding: 10px;
      }
      .start {
        scroll-snap-align: start;
      }
      .center {
        scroll-snap-align: center;
      }
      .end {
        scroll-snap-align: end;
      }
    </style>
    <div id="snap"><div class="start">A</div><div class="center">B</div><div class="end"></div></div>
  `, 'Verifies that Overlay.setShowScrollSnapOverlays works.');

  await dp.DOM.enable();
  await dp.Overlay.enable();

  const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
  const {result: {nodeId}} = await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#snap'});

  async function waitForAnimationFrame() {
    await session.evaluateAsync(() => {
      return new Promise(resolve => requestAnimationFrame(resolve));
    });
  }

  async function getDrawScrollSnapHighlightCommands() {
    await waitForAnimationFrame();
    return await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        // Multiple frames could be rendered but they should to be identical.
        const commands = window.commands;
        window.commands = [];
        return JSON.stringify(commands.filter(command => command.name === 'drawScrollSnapHighlight').shift(), null, 2);
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
  testRunner.log(await getDrawScrollSnapHighlightCommands());

  const black = {
    r: 0,
    g: 0,
    b: 0,
    a: 1,
  };
  await dp.Overlay.setShowScrollSnapOverlays({
    scrollSnapHighlightConfigs: [{
      nodeId,
      scrollSnapContainerHighlightConfig: {
        snapportBorder: {
          color: black,
        },
        snapAreaBorder: {
          color: black,
        },
        scrollMarginColor: black,
        scrollPaddingColor: black,
      },
    }]
  });

  testRunner.log('Overlay rendered:');
  testRunner.log(await getDrawScrollSnapHighlightCommands());

  testRunner.completeTest();
});

