(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} =
      await testRunner.startHTML(`
       <button id="button" interestfor="target">Button</button>
       <div id="target" popover=hint>Target hint</div>
      `,
                                 'Tests the DOM.forceShowInterest method');

  await dp.Runtime.enable();
  await dp.DOM.enable();
  const doc = await dp.DOM.getDocument();

  async function getNodeId(selector) {
    const {result: {nodeId}} =
        await dp.DOM.querySelector({nodeId: doc.result.root.nodeId, selector});
    return nodeId;
  }

  const buttonId = await getNodeId('#button');

  async function checkState(description) {
    const state = await dp.Runtime.evaluate({
      returnByValue: true,
      expression: `({
        popoverOpen: document.getElementById('target').matches(':popover-open'),
        buttonInterestSource: document.getElementById('button').matches(':interest-source'),
        targetInterestTarget: document.getElementById('target').matches(':interest-target')
      })`
    });
    testRunner.log(state.result.result, description + ': ');
  }

  testRunner.log('1. Initial state:');
  await checkState('State');

  testRunner.log('\n2. Call DOM.forceShowInterest(enable: true):');
  const {error: enableErr} =
      await dp.DOM.forceShowInterest({nodeId: buttonId, enable: true});
  if (enableErr) {
    testRunner.log('Error: ' + enableErr.message);
  }
  await checkState('State after forceShowInterest(true)');

  testRunner.log('\n3. Call DOM.forceShowInterest(enable: false):');
  const {error: disableErr} =
      await dp.DOM.forceShowInterest({nodeId: buttonId, enable: false});
  if (disableErr) {
    testRunner.log('Error: ' + disableErr.message);
  }
  await checkState('State after forceShowInterest(false)');

  testRunner.completeTest();
})
