(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
    @function --f(--x) {
      --y: var(--x);
      @supports (color: green) {
        @media (width > 300px) {
          --y: green;
        }
      }
      result: var(--y);
    }
    #test1 {
      color: --f(blue);
    }
    @function --inner(--x) {
      result: var(--x);
    }
    @function --outer(--x) {
      result: orange;
      @container (width > 300px) {
        result: --inner(var(--x));
      }
    }
    #test3 {
      color: --outer(yellow);
    }
  </style>
  <body>
  <div id="test1">test1</div>
  <div>
    <style>
      @function --f2(--ignored, --used) {
        result: var(--used);
      }
      #test2 {
        color: --f2(blue, turquoise);
      }
    </style>
    <div id="test2">test2</div>
  </div>
  <div id="test3">test3</div>
  </body>
`,
      'Verify that functions are reported properly.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;

  for (const selector of ['#test1', '#test2', '#test3']) {
    const test = await dp.DOM.querySelector({
      nodeId: documentNodeId,
      selector,
    });
    const testId = test.result.nodeId;

    const matchedStyles =
        await dp.CSS.getMatchedStylesForNode({nodeId: testId});

    testRunner.log('Functions for ' + selector);
    const functionRules = matchedStyles.result.cssFunctionRules;
    functionRules.sort(
        (a, b) => JSON.stringify(a).localeCompare(JSON.stringify(b)));
    for (const functionRule of functionRules) {
      testRunner.log(
          functionRule, 'function rule: ', [], ['styleSheetId', 'sourceURL']);
    }
  }
  testRunner.completeTest();
});
