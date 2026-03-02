(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
<!DOCTYPE html>
<html>
  <head>
    <link rel="stylesheet" href="${testRunner.url('resources/css-function-double-linked.css')}"/>
  </head>

  <body>
    <div id="test">
      <link rel="stylesheet" href="${testRunner.url('resources/css-function-double-linked.css')}"/>
      <style>
        div {
          width: --foo();
        }
      </style>
    </div>
  </body>
</html>
`, 'Load the same stylesheet twice, and observe collected functions');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const document = await dp.DOM.getDocument({});
  const documentNodeId = document.result.root.nodeId;

  const test = await dp.DOM.querySelector({
    nodeId: documentNodeId,
    selector: '#test',
  });
  const testId = test.result.nodeId;

  testRunner.log('Calling getMatchedStylesForNode ...');
  try {
    const matchedStyles = await dp.CSS.getMatchedStylesForNode({nodeId: testId});
    testRunner.log('Call returned.');
    if (matchedStyles.result.cssFunctionRules) {
      testRunner.log('Found ' + matchedStyles.result.cssFunctionRules.length + ' function rule(s).');
    } else {
      testRunner.log('No function rules found.');
    }
  } catch (e) {
    testRunner.log('Caught error: ' + e);
  }

  testRunner.completeTest();
});