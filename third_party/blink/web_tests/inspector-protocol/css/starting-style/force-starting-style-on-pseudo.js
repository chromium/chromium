(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startHTML(`
    <style>
      #target.active::after {
        content: '';
        opacity: 1;
        @starting-style {
          opacity: 0;
        }
      }
    </style>
    <body>
      <div id="target"></div>
    </body>
  `, 'Test that forcing starting style on pseudo-element works');

  await dp.DOM.enable();
  await dp.CSS.enable();

  async function requestDocumentNodeId() {
    const {result} = await dp.DOM.getDocument({});
    return result.root.nodeId;
  }
  async function requestNodeId(nodeId, selector) {
    const {result} = await dp.DOM.querySelector({nodeId, selector});
    return result.nodeId;
  }
  async function waitFrames(count) {
    await session.evaluateAsync(`
      new Promise(resolve => {
        let remaining = ${count};
        function step() {
          if (remaining-- <= 0) {
            resolve();
          } else {
            requestAnimationFrame(step);
          }
        }
        step();
      })
    `);
  }

  function hasStartingStyleRule(matchedStyles) {
    for (const match of matchedStyles.matchedCSSRules) {
      const rule = match.rule;
      if (rule.startingStyles && rule.startingStyles.length > 0) {
        return true;
      }
    }
    return false;
  }

  const documentNodeId = await requestDocumentNodeId();

  const pseudoAdded = dp.DOM.oncePseudoElementAdded();

  const nodeId = await requestNodeId(documentNodeId, '#target');

  // Trigger pseudo-element creation
  await session.evaluate('document.querySelector("#target").classList.add("active")');

  const { params: { pseudoElement }} = await pseudoAdded;
  const pseudoNodeId = pseudoElement.nodeId;

  // Wait for starting style to be naturally gone
  await waitFrames(2);

  async function getPseudoOpacity() {
    return await session.evaluate('getComputedStyle(document.querySelector("#target"), "::after").opacity');
  }

  testRunner.log('--- NOT FORCED ---');
  {
    const { result: matchedStyles } = await dp.CSS.getMatchedStylesForNode({ nodeId: pseudoNodeId });
    if (!hasStartingStyleRule(matchedStyles)) {
      testRunner.log('SUCCESS: starting-style rule not found');
    } else {
      testRunner.log('FAILED: starting-style rule found');
    }
    const opacity = await getPseudoOpacity();
    testRunner.log(`Computed opacity: ${opacity}`);
  }

  testRunner.log('--- FORCED ---');
  await dp.CSS.forceStartingStyle({ nodeId: pseudoNodeId, forced: true });
  {
    const { result: matchedStyles } = await dp.CSS.getMatchedStylesForNode({ nodeId: pseudoNodeId });
    const hasStartingStyle = matchedStyles.matchedCSSRules.some(r => r.rule.startingStyles && r.rule.startingStyles.length > 0);
    if (hasStartingStyle) {
      testRunner.log('SUCCESS: starting-style rule found');
    } else {
      testRunner.log('FAILED: starting-style rule not found');
    }
    const opacity = await getPseudoOpacity();
    testRunner.log(`Computed opacity: ${opacity}`);
  }

  testRunner.log('--- DISABLED ---');
  await dp.CSS.forceStartingStyle({ nodeId: pseudoNodeId, forced: false });
  {
    const { result: matchedStyles } = await dp.CSS.getMatchedStylesForNode({ nodeId: pseudoNodeId });
    if (!hasStartingStyleRule(matchedStyles)) {
      testRunner.log('SUCCESS: starting-style rule not found');
    } else {
      testRunner.log('FAILED: starting-style rule found');
    }
    const opacity = await getPseudoOpacity();
    testRunner.log(`Computed opacity: ${opacity}`);
  }

  testRunner.completeTest();
});
