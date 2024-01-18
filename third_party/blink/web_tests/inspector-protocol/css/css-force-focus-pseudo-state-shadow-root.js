(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
  <style>
    #host:focus-within {
      border: 1px solid black;
    }
  </style>
  <template id="tmpl">
    <style> #inner:focus { background: red; } </style>
    <input id="inner" value="hi!"></input>
  </template>
  <div id="host"></div>
  `, 'Test CSS.forcePseudoStates method for :focus setting :focus-within state across shadow root boundaries');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  for (const mode of ['open', 'closed']) {
    testRunner.log(`Shadow mode: ${mode}`);
    await session.evaluate(() => {
      // Reset the #host element by re-creating it.
      document.body.removeChild(document.querySelector('#host'));
      const newHost = document.createElement('div');
      newHost.id = 'host';
      document.body.appendChild(newHost)
    });

    const documentNodeId = await cssHelper.requestDocumentNodeId();
    await cssHelper.requestNodeId(documentNodeId, '#host');

    session.evaluate((mode) => {
      const shadowRoot = document.querySelector('#host').attachShadow({mode});
      const template = document.querySelector('#tmpl');
      const clone = document.importNode(template.content, true);
      shadowRoot.appendChild(clone);
    }, mode);

    const shadowRootPushed = await dp.DOM.onceShadowRootPushed();
    const nodeId = await cssHelper.requestNodeId(shadowRootPushed.params.root.nodeId, '#inner');

    async function getInnerBackgroudColor() {
      const { result } = await dp.CSS.getMatchedStylesForNode({ nodeId });
      const matchedRule = result.matchedCSSRules.find(match => match.rule.selectorList.text== '#inner:focus');
      if (!matchedRule) {
        return 'default';
      }
      return matchedRule.rule.style.cssText;
    }

    async function getHostBorder() {
      return await session.evaluate(() => {
        return window.getComputedStyle(document.querySelector('#host')).border;
      });
    }

    testRunner.log("#inner background color without forced focus: " + await getInnerBackgroudColor());
    testRunner.log("#host border without forced focus: " + await getHostBorder());

    await dp.CSS.forcePseudoState({nodeId, forcedPseudoClasses: ['focus']});

    testRunner.log("#inner background color with forced focus: " + await getInnerBackgroudColor());
    testRunner.log("#host border with forced focus: " + await getHostBorder());
  }

  testRunner.completeTest();
});
