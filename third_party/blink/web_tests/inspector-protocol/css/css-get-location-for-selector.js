(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/css-get-location-for-selector.html',
      'Test css.getLocationForSelector method');

  // style sheet #1
  // styles defined in file css-get-location-for-selector.css
  const selectorDivClass1 = 'div.class1';
  const selectorDivClass11 = '& .class11';  // nested selector
  const selectorDivClass1Class2 = 'div.class1 .class2';
  const selectorDivClass6 =
      'div.class6';  // selector defined in the middle of a long line
  const selectorSecondInList = '.second-in-selector-list';
  // style sheet #2
  // style defined in file css-get-location-for-selector.html
  const selectorDivItem3 = 'div#item3';
  const selectorDivItem31 = 'div #item3';
  // constructed style sheet
  const selectorClass7 = '.class7';

  const kTotalNumberOfStyleSheets = 3;
  let numberOfAddedStyleSheets = 0;
  let styleSheetId1, styleSheetId2, styleSheetId3;
  const allStyleSheetsAddedPromise = new Promise(resolve => {
    dp.CSS.onStyleSheetAdded((e) => {
      const header = e.params.header;
      numberOfAddedStyleSheets++;

      if (header.isInline) {
        styleSheetId2 = header.styleSheetId;
      } else if (header.isConstructed) {
        styleSheetId3 = header.styleSheetId;
      } else {
        styleSheetId1 = header.styleSheetId;
      }

      if (numberOfAddedStyleSheets === kTotalNumberOfStyleSheets) {
        resolve();
      }
    });
  });

  dp.DOM.enable();
  dp.CSS.enable();

  // Wait until all stylesheets are added for the test
  // otherwise there is a possible race condition in
  // some stylesheets being loading and getMediaQueries
  // request being made.
  await allStyleSheetsAddedPromise;

  if (!styleSheetId1 || !styleSheetId2 || !styleSheetId3) {
    testRunner.log('failed to get style sheet IDs.');
    testRunner.completeTest();
  }

  // get line ranges for 'div.class1'
  await logSelectorRanges(selectorDivClass1, styleSheetId1);

  // get line ranges for 'div.class1 class11', nested selector
  await logSelectorRanges(selectorDivClass11, styleSheetId1);

  // get line ranges for 'div.class1 .class2'
  await logSelectorRanges(selectorDivClass1Class2, styleSheetId1);

  // get line ranges for 'div.class6', a selector defined in the middle of a
  // long line
  await logSelectorRanges(selectorDivClass6, styleSheetId1);

  // get line ranges for `.second-in-selector-list`
  await logSelectorRanges(selectorSecondInList, styleSheetId1);

  // get line ranges for 'div#item3'
  await logSelectorRanges(selectorDivItem3, styleSheetId2);

  // get line ranges for 'div #item3'
  await logSelectorRanges(selectorDivItem31, styleSheetId2);

  // selector defined in constructed stylesheet
  await logSelectorRanges(selectorClass7, styleSheetId3);

  // Not present in the stylesheet
  await logSelectorRanges('.not-in-stylesheet', styleSheetId3);

  testRunner.completeTest();

  async function logSelectorRanges(selector, styleSheetId) {
    const response = await dp.CSS.getLocationForSelector(
        {styleSheetId, selectorText: selector});
    if (response.error) {
      // Remove the machine-specific filepath of the offending stylesheet
      const nonFlakyError = response.error.message.replace(/( in style sheet).*/, '$1');
      testRunner.log(nonFlakyError);
      return;
    }

    const ranges = response.result.ranges;
    for (let i = 0; i < ranges.length; i++) {
      testRunner.log(`range ${i + 1} of selector '${selector}':`);
      testRunner.log(`    start line: ${ranges[i].startLine}`);
      testRunner.log(`    end line: ${ranges[i].endLine}`);
      testRunner.log(`    start column: ${ranges[i].startColumn}`);
      testRunner.log(`    end column: ${ranges[i].endColumn}`);
    }
  }
});
