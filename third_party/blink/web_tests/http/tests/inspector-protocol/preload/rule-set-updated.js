(async function(testRunner) {
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
    </head>
    <body>
    </body>
    </html>
`;

  async function testEnabled() {
    const {dp, session, page} = await testRunner.startBlank(
        `Tests that Preload.ruleSetUpdated and Preload.ruleSetDeleted are dispatched.`);

    await dp.Preload.enable();

    await new Promise(resolve => {
      let count = 2;
      dp.Preload.onRuleSetUpdated(ruleSet => {
        ruleSet.params.ruleSet.sourceText =
            JSON.parse(ruleSet.params.ruleSet.sourceText);
        testRunner.log(ruleSet);

        --count;
        if (count === 0) {
          resolve();
        }
      });
      void page.loadHTML(html);
    });

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
