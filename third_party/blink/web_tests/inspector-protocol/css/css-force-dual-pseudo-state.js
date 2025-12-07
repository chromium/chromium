(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
<link rel='stylesheet' href='${
          testRunner.url('resources/css-force-specific-pseudo-state.css')}'/>
<div>
  <span id="test1" class="testcase">Some immutable text</span>
  <span id="test2" class="testcase" contenteditable>Some editable text</span>
  <span contenteditable
    ><i id="test3" class="testcase">Children are also editable</i></span
  >
  <textarea id="test4" class="testcase">A textarea</textarea>
  <textarea id="test5" class="testcase" readonly>
An immutable textarea</textarea
  >
  <textarea id="test6" class="testcase" disabled>
A disabled textarea</textarea
  >
  <textarea id="test7" class="testcase" readonly disabled>
An immutable disabled textarea</textarea
  >
  <input id="test8" class="testcase" type="text" value="A text input" />
  <input id="test9"
    class="testcase"
    type="text"
    readonly
    value="An immutable text input"
  />
  <input
    id="test10"
    class="testcase"
    type="text"
    disabled
    value="A disabled text input"
  />
  <input
    id="test11"
    class="testcase"
    type="text"
    readonly
    disabled
    value="An immutable disabled text input"
  />
  <button id="test12" class="testcase">A button</button>
  <button id="test13" class="testcase" disabled>A disabled button</button>
  <input id="test14" class="testcase" type="email" value="Not a valid email input" required/>
  <input id="test15" class="testcase" type="number" value="12" min="1" max="10"/>
  <input id="test16" class="testcase" type="number" value="2" min="1" max="10"/>
</div>`,
      'Test CSS.forcePseudoStates method for dual pseudo state pairs');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);

  const documentNodeId = await cssHelper.requestDocumentNodeId();

  async function getPropertyValueForId(id, property) {
    return await session.evaluate((id, property) => {
      return window.getComputedStyle(document.getElementById(id))
          .getPropertyValue(property);
    }, id, property);
  }

  const duals = [
    ['enabled', 'disabled'],
    ['valid', 'invalid'],
    ['user-valid', 'user-invalid'],
    ['required', 'optional'],
    ['read-only', 'read-write'],
    ['in-range', 'out-of-range'],
  ];

  const pseudoClasses = duals.flat();
  const dualOf = new Map([...duals, ...duals.map(([a, b]) => [b, a])]);

  const getPseudoClasses =
      async (id) => {
    const result = [];
    for (const pseudoClass of pseudoClasses) {
      const value = await getPropertyValueForId(id, `--${pseudoClass}-applied`);
      if (value === '\'true\'') {
        result.push(pseudoClass);
      }
    }
    return result;
  }

  for (let i = 1; i <= 16; ++i) {
    const nodeId = await cssHelper.requestNodeId(documentNodeId, `#test${i}`);
    const id = `test${i}`;
    testRunner.log(`TESTCASE: ${id}`);
    const originalStates = await getPseudoClasses(id);
    const dualStates = originalStates.map(x => dualOf.get(x));
    testRunner.log('Original states: ' + originalStates.join(' '));
    await dp.CSS.forcePseudoState({nodeId, forcedPseudoClasses: dualStates});
    const newStates = await getPseudoClasses(id);
    testRunner.log('New states: ' + newStates.join(' '));
    testRunner.log('Should match: ' + dualStates.join(' '));
  }

  await dp.CSS.disable();
  await dp.DOM.disable();

  testRunner.log(
      'Didn\'t fail after disabling the CSS agent (https://crbug.com/1123526).');

  testRunner.completeTest();
});
