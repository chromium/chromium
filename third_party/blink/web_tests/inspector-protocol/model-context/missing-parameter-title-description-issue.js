(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Verify that an issue is reported for missing parameter title/description');

  await dp.Audits.enable();
  await dp.DOM.enable();

  const issuePromise = dp.Audits.onceIssueAdded(issue => {
    return issue.params.issue.code === 'GenericIssue';
  });

  await page.loadHTML(`
    <!DOCTYPE html>
    <form id="form" toolname="mytool" tooldescription="perform task">
      <input id="text1" name="text1" type="text">
    </form>
    <script>
      if (window.navigator.modelContextTesting) {
        window.navigator.modelContextTesting.listTools();
      }
    </script>
  `);

  const issue = await issuePromise;
  const nodeId = issue.params.issue.details.genericIssueDetails.violatingNodeId;
  const response = await dp.DOM.describeNode({backendNodeId: nodeId});
  const node = response.result.node;
  let attributes = node.attributes || [];
  testRunner.log(`Node name: ${node.nodeName}`);
  testRunner.log(`Node attribute data: ${attributes.join(' ')}`);
  testRunner.log("Issue code: " + issue.params.issue.code);
  testRunner.log("Issue type: " + issue.params.issue.details.genericIssueDetails.errorType);

  testRunner.completeTest();
});
