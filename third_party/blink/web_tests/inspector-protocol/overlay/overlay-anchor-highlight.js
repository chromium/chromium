(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
    <style>
      #container {
        position: relative;
        width: 400px;
        height: 400px;
      }
      #anchor {
        anchor-name: --test-anchor;
        position: absolute;
        left: 50px;
        top: 50px;
        width: 100px;
        height: 80px;
      }
      #anchored {
        position: absolute;
        position-anchor: --test-anchor;
        position-area: bottom right;
        width: 120px;
        height: 60px;
      }
      #abspos {
        position: absolute;
        top: 20px;
        left: 30px;
        right: 40px;
        bottom: 50px;
      }
    </style>
    <div id="container">
      <div id="anchor"></div>
      <div id="anchored"></div>
      <div id="abspos"></div>
    </div>
  `,
      'Verifies that Overlay.highlightNode works with imcbHighlightConfig.');

  await dp.DOM.enable();
  await dp.Overlay.enable();

  const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
  const {result: {nodeId: anchoredNodeId}} = await dp.DOM.querySelector(
      {nodeId: documentNodeId, selector: '#anchored'});
  const {result: {nodeId: absposNodeId}} =
      await dp.DOM.querySelector({nodeId: documentNodeId, selector: '#abspos'});

  async function waitForAnimationFrame() {
    await session.evaluateAsync(() => {
      return new Promise(resolve => requestAnimationFrame(resolve));
    });
  }

  async function getHighlightNodeImcbInfo() {
    await waitForAnimationFrame();
    return await session.evaluate(() => {
      return internals.evaluateInInspectorOverlay(`(function () {
        const commands = window.commands;
        window.commands = [];
        const highlight = commands.filter(command => command.name === 'drawHighlight').shift();
        return JSON.stringify(highlight ? highlight.data.imcbInfo : null, null, 2);
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

  const purple = {r: 127, g: 32, b: 210, a: 1};
  const blue = {r: 26, g: 115, b: 232, a: 1};

  await dp.Overlay.highlightNode({
    nodeId: anchoredNodeId,
    highlightConfig: {
      imcbHighlightConfig: {
        imcbBorderColor: purple,
        anchorBorderColor: blue,
        showPositionAreaGrid: true,
        positionAreaGridLineColor: blue,
      },
    },
  });

  testRunner.log('Anchor highlight rendered:');
  testRunner.log(await getHighlightNodeImcbInfo());

  await dp.Overlay.highlightNode({
    nodeId: absposNodeId,
    highlightConfig: {
      imcbHighlightConfig: {
        imcbBorderColor: purple,
      },
    },
  });

  testRunner.log('Abspos IMCB highlight rendered:');
  testRunner.log(await getHighlightNodeImcbInfo());

  testRunner.completeTest();
});
