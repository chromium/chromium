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

  async function testEnabled() {
    const {dp, session, page} = await testRunner.startBlank(
        `Tests that Preload.ruleSetUpdated and Preload.ruleSetDeleted are dispatched.`);

    await dp.Preload.enable();

    void page.loadHTML(html);

    for (let count = 0; count < 5; ++count) {
      const {ruleSet} = (await dp.Preload.onceRuleSetUpdated()).params;

      // Format sourceText.
      ruleSet.sourceText = ruleSet.errorType === undefined ?
          JSON.parse(ruleSet.sourceText) :
          // Prevent failures due to non visible differences coming from LF.
          ruleSet.sourceText.replaceAll(/[\n ]+/g, '');
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
