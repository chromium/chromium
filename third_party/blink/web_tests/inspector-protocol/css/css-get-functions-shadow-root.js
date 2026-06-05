// TODO(leese): Add case where function is shadow calls function in host.


(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
    body {
      --color-1: --used-in-both();
      --color-2: --used-in-host();
      --invalid-color-1: --invalid-in-host();
    }
    @function --used-in-both() {
      result: red;
    }
    @function --used-in-host() {
      result: --used-indirectly();
    }
    @function --does-not-inherit() {
      result: blue;
    }
    @function --used-in-shadow() {
      result: yellow;
    }
    @function --used-indirectly() {
      result: green;
    }
    @function --defined-in-host() {
      result: --used-indirectly-in-host();
    }
    @function --used-indirectly-in-host() {
      result: white;
    }
  </style>
  <body>
    <div id=host>
      <template shadowrootmode=open>
        <style>
          @function --used-in-both() {
            result: orange;
          }
          @function --used-in-shadow() {
            result: --used-indirectly();
          }
          @function --used-indirectly() {
            result: purple;
          }
          @function --used-in-host() {
            result: turquoise;
          }
          @function --invalid-in-host() {
            result: black;
          }
          @function --used-indirectly-in-host() {
            result: magenta;
          }
          #test {
            --color-3: --used-in-shadow();
            --color-4: --used-in-both();
            --color-5: --defined-in-host();
          }
        </style>
        <div id="test">test</div>
      </template>
    </div>
  </body>
`,
      'Verify that functions are reported properly from a shadow root.');
  await dp.DOM.enable();
  await dp.CSS.enable();

  const document = await dp.DOM.getDocument({depth: -1});
  const shadowRootNodeId = document.result.root.children[0]
                               .children[1]
                               .children[0]
                               .shadowRoots[0]
                               .nodeId;

  const documentTreeScopeId = document.result.root.backendNodeId;
  const shadowRootTreeScopeId = document.result.root.children[0]
                                    .children[1]
                                    .children[0]
                                    .shadowRoots[0]
                                    .backendNodeId;

  const test = await dp.DOM.querySelector({
    nodeId: shadowRootNodeId,
    selector: '#test',
  });
  const testId = test.result.nodeId;

  const matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: testId});
  const {result: {computedStyle}} =
      await dp.CSS.getComputedStyleForNode({nodeId: testId});
  testRunner.log('Computed style:');
  computedStyle.filter(c => c.name.startsWith('--'))
      .map(c => `${c.name}: ${c.value}`)
      .sort()
      .forEach(c => testRunner.log(c));

  testRunner.log('Functions for #test:');
  const functionRules = matchedStyles.result.cssFunctionRules;
  functionRules.sort(
      (a, b) => JSON.stringify(a).localeCompare(JSON.stringify(b)));
  for (const functionRule of functionRules) {
    const origin = functionRule.originTreeScopeNodeId === documentTreeScopeId ?
        'document' :
        functionRule.originTreeScopeNodeId === shadowRootTreeScopeId ?
        'shadow' :
        'unknown';
    testRunner.log(`@function ${functionRule.name.text}() {${
        functionRule.children[0].style.cssText}} /* from ${origin} */`);
  }
  testRunner.completeTest();
});
