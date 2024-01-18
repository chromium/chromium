(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Test reported issues for incorrect property rules');

  await dp.DOM.enable();
  await dp.CSS.enable();
  await dp.Audits.enable();

  const audits = [];
  dp.Audits.onIssueAdded(issue => {
    if (issue.params.issue.code === 'PropertyRuleIssue') {
      // url is sensitive to where the test is running, so sanitize it:
      if (issue?.params?.issue?.details?.propertyRuleIssueDetails
              ?.sourceCodeLocation?.url?.endsWith(
                  'inspector-protocol-page.html')) {
        issue.params.issue.details.propertyRuleIssueDetails.sourceCodeLocation
            .url = 'inspector-protocol-page.html';
      }
      audits.push(issue)
    }
  });

  await page.loadHTML(`
  <style>
  @property --incorrect-initial-value {
    syntax: "<number>";
    inherits: false;
    initial-value: red;
  }
  @property --missing-initial-value {
    syntax: "<number>";
    inherits: false;
  }
  @property --incorrect-syntax {
    syntax: "<lengthy>";
    inherits: false;
    initial-value: 100%;
  }
  @property --missing-inherits {
    syntax: "<number>";
    initial-value: 0;
  }
  @property --invalid-inherits {
    syntax: "<number>";
    initial-value: 0;
    inherits: red;
  }
  @property --missing-everything {
  }
  @property invalid-property-name {
  }
  </style>

  <div>div</div>
  `);


  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});
  const {result: {cssPropertyRules}} =
      await dp.CSS.getMatchedStylesForNode({nodeId});
  testRunner.log(audits);
  testRunner.log(cssPropertyRules);
  testRunner.completeTest();
});
