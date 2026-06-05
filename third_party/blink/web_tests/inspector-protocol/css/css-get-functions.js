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
    @function --after-color() {
      result: red;
    }
    #test4::after {
      color: --after-color();
      content: "after";
    }
    @function --highlight-color() {
      result: yellow;
    }
    @function --g() {
      result: black;
    }
    .test5-container {
      color: --g();
    }
    .test5-container::selection {
      color: --highlight-color();
    }
    .test5-container::after { /* Not inherited */
      color: --after-color();
      content: "after";
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
  <div id="test4">test4</div>
  <div class="test5-container"><div id="test5">test5</div></div>
  </body>
`,
      'Verify that functions are reported properly.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;

  for (const selector of ['#test1', '#test2', '#test3', '#test4', '#test5']) {
    const test = await dp.DOM.querySelector({
      nodeId: documentNodeId,
      selector,
    });
    const testId = test.result.nodeId;

    const matchedStyles =
        await dp.CSS.getMatchedStylesForNode({nodeId: testId});

    testRunner.log('Functions for ' + selector);
    const functionRules = matchedStyles.result.cssFunctionRules ?? [];
    functionRules.sort(
        (a, b) => JSON.stringify(a).localeCompare(JSON.stringify(b)));
    for (const functionRule of functionRules) {
      testRunner.log(
          functionRule, 'function rule: ', [],
          ['styleSheetId', 'sourceURL', 'originTreeScopeNodeId']);
    }
  }
  testRunner.completeTest();
});
