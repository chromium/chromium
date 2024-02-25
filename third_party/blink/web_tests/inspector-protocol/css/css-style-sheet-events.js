(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(`
      <style>
      #test {
          box-sizing: border-box;
      }
      </style>
      <article id='test'></article>`,
      'The test verifies events fire for adding, changing, and removing CSSStyleSheets.');

  const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  const cssHelper = new CSSHelper(testRunner, dp);
  await cssHelper.requestDocumentNodeId();

  // Add Event
  dp.CSS.enable();
  const addEvent = await dp.CSS.onceStyleSheetAdded();
  testRunner.log(addEvent, '', [ ...TestRunner.stabilizeNames, 'length' ]);
  const styleSheetId = addEvent.params.header.styleSheetId;

  // Change Event
  async function addRuleAndUndo(options) {
    options.styleSheetId = styleSheetId;
    await dp.CSS.addRule(options);
    await dp.DOM.undo();
  }
  addRuleAndUndo({
    location: { startLine: 0, startColumn: 0, endLine: 0, endColumn: 0 },
    ruleText: `#test { content: 'EDITED'; }`,
  });
  const changedEvent = await dp.CSS.onceStyleSheetChanged();
  testRunner.log(changedEvent);

  await dp.CSS.disable();
  const addEventAfterChangePromise = dp.CSS.onceStyleSheetAdded();
  await dp.CSS.enable();
  const addEventAfterChange = await addEventAfterChangePromise;
  testRunner.log(addEventAfterChange, '', [ ...TestRunner.stabilizeNames, 'length' ]);

  // Remove event
  session.evaluate(`
      [...document.head.getElementsByTagName('style')].forEach(item => item.remove())
  `);
  const removeEvent = await dp.CSS.onceStyleSheetRemoved();
  testRunner.log(removeEvent);
  testRunner.completeTest();
});
