(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
<!DOCTYPE html>
<style>
.base, .base::picker(select) {
  appearance: base-select;
}
</style>

<select class=base>
  <button>button</button>
  <option>option</option>
  <optgroup>
    <legend>legend</legend>
  </optgroup>
</select>

<select class=auto>
  <button>button</button>
  <option>option</option>
  <optgroup>
    <legend>legend</legend>
  </optgroup>
</select>
`, 'The test verifies functionality of protocol method CSS.isAppearanceBase.');

  await dp.DOM.enable();
  await dp.CSS.enable();
  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  const selectors = [
    '.base',
    '.base button',
    '.base option',
    '.base optgroup',
    '.base legend',
    '.auto',
    '.auto button',
    '.auto option',
    '.auto optgroup',
    '.auto legend',
  ];

  for (const selector of selectors) {
    const nodeId = await cssHelper.requestNodeId(documentNodeId, selector);
    const response = await dp.CSS.getComputedStyleForNode({nodeId});
    testRunner.log(`"${selector}" isAppearanceBase: ${response.result.extraFields.isAppearanceBase}`);
  }

  testRunner.completeTest();
});
