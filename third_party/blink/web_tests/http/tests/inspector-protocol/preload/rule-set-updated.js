(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const html = `
    <!DOCTYPE html>
    <html>
    <head>
      <script type="speculationrules" id="prefetch">
        {
          "prefetch":[
            {
              "source": "list",
              "urls": ["/subresource.js"]
            }
          ]
        }
      </script>
      <script type="speculationrules" id="prerender">
        {
          "prerender":[
            {
              "source": "list",
              "urls": ["/page.html"]
            }
          ]
        }
      </script>
      <script type="speculationrules" id="invalid-json">
        {
          "prefetch":[
      </script>
      <script type="speculationrules" id="not-object">
        "invalid"
      </script>
      <script type="speculationrules" id="contains-invalid-rule">
        {
          "prefetch": [
            {
              "source": "list",
              "urls": ["/subresource.js"]
            }
          ],
          "prerender": "invalid"
        }
      </script>
    </head>
    <body>
    </body>
    </html>
`;

  async function backendNodeIdToNodeId(dp, backendNodeId) {
    const nodeIds = (await dp.DOM.pushNodesByBackendIdsToFrontend({
                      backendNodeIds: [backendNodeId]
                    })).result.nodeIds;
    if (nodeIds.length !== 1) {
      throw new Error('unreachable');
    }
    return nodeIds[0];
  }

  async function testEnabled() {
    const {dp, session, page} = await testRunner.startBlank(
        `Tests that Preload.ruleSetUpdated and Preload.ruleSetDeleted are dispatched.`);

    await dp.Preload.enable();
    await dp.DOM.enable();

    const loadHTMLPromise = page.loadHTML(html);

    const selectors = [
      '#prefetch',
      '#prerender',
      '#invalid-json',
      '#not-object',
      '#contains-invalid-rule',
    ];

    let ruleSets = [];
    for (let count = 0; count < selectors.length; ++count) {
      const {ruleSet} = (await dp.Preload.onceRuleSetUpdated()).params;

      ruleSets.push(ruleSet);
    }

    await loadHTMLPromise;

    const documentNodeId = (await dp.DOM.getDocument()).result.root.nodeId;
    let mapNodeIdToSelector = {};
    for (const selector of selectors) {
      const nodeId =
          (await dp.DOM.querySelector({nodeId: documentNodeId, selector}))
              .result.nodeId;
      mapNodeIdToSelector[nodeId] = selector;
    }

    for (const ruleSet of ruleSets) {
      // Format sourceText.
      ruleSet.sourceText = ruleSet.errorType === undefined ?
          JSON.parse(ruleSet.sourceText) :
          // Prevent failures due to non visible differences coming from LF.
          ruleSet.sourceText.replaceAll(/[\n ]+/g, '');

      // Supplement corresponding selector.
      ruleSet._selector = mapNodeIdToSelector[await backendNodeIdToNodeId(
          dp, ruleSet.backendNodeId)];

      testRunner.log(ruleSet);
    }

    session.evaluate('document.getElementById("prefetch").remove();');
    testRunner.log(await dp.Preload.onceRuleSetRemoved());
  }

  async function testDisabled() {
    const {dp, session, page} = await testRunner.startBlank(
        `Tests that Preload.ruleSetUpdated and Preload.ruleSetDeleted are not dispatched.`);

    await dp.Preload.enable();
    await dp.Preload.disable();

    dp.Preload.onRuleSetUpdated(_ => {
      throw new Error('Expect not called.');
    });
    await page.loadHTML(html);
  }

  testRunner.runTestSuite([
    testEnabled,
    testDisabled,
  ]);
});
