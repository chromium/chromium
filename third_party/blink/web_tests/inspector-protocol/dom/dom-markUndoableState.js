(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <input></input>
  `, 'Tests that DOM.markUndoableState does not crash when DOM is disabled.');

  await dp.DOM.markUndoableState();
  testRunner.log('Did not crash');
  testRunner.completeTest();
})
