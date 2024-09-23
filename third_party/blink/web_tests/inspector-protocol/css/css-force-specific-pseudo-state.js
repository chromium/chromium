(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
   <link rel='stylesheet' href='${testRunner.url('resources/css-force-specific-pseudo-state.css')}'/>
    <div id="div">t1</div>
    <div id="editableDiv" contenteditable="true">t2</div>`,
   'Test CSS.forcePseudoStates method for specific pseudo states');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();
  const divNodeId = await cssHelper.requestNodeId(documentNodeId, '#div');
  const editableDivId = await cssHelper.requestNodeId(documentNodeId, '#editableDiv');

  async function getPropertyValueForId(id, property) {
    return await session.evaluate((id, property) => {
      return window.getComputedStyle(document.getElementById(id)).getPropertyValue(property);
    }, id, property);
  }

  // TODO(crbug.com/332914922): Also add :link and tests for :visited when the bug is fixed.
  const pseudoClasses = ['enabled', 'disabled',
    'valid', 'invalid', 'user-valid', 'user-invalid', 'required', 'optional', 'read-only',
    'read-write', 'in-range', 'out-of-range', 'checked', 'indeterminate',
    'placeholder-shown', 'autofill'];
  const pseudoClassProps = pseudoClasses.map(x => `--${x}-applied`);

  const logAllPseudoClassPropsForDiv = async () => {
    testRunner.log('\nStates for div:');
    for (const pseudoClassProp of pseudoClassProps) {
      testRunner.log(`${pseudoClassProp}=${await getPropertyValueForId('div', pseudoClassProp)}`);
    }
  }

  testRunner.log('States before forcing pseudo states:');
  await logAllPseudoClassPropsForDiv();
  testRunner.log(`\nStates for editableDiv:\n--read-only-applied=${await getPropertyValueForId('editableDiv', '--read-only-applied')}`);

  // We test both a 'div' and an 'editable div' since a 'div' has the :read-only pseudo state
  // enabled by default unless the contenteditable attribute is set to true.
  await dp.CSS.forcePseudoState({nodeId: divNodeId, forcedPseudoClasses: pseudoClasses});
  await dp.CSS.forcePseudoState({nodeId: editableDivId, forcedPseudoClasses: ['read-only']});

  testRunner.log('\nStates after forcing pseudo states:');
  await logAllPseudoClassPropsForDiv();
  testRunner.log(`\nStates for editableDiv:\n--read-only-applied=${await getPropertyValueForId('editableDiv', '--read-only-applied')}`);

  await dp.CSS.disable();
  await dp.DOM.disable();

  testRunner.log("Didn't fail after disabling the CSS agent (https://crbug.com/1123526).");

  testRunner.completeTest();
});
