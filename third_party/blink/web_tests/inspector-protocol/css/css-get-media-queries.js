(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' media='print and (min-width: 8.5in)' href='${testRunner.url('./resources/media-queries.css')}'></link>
<link rel='stylesheet' href='${testRunner.url('./resources/active-media-queries.css')}'></link>
<style>
@media screen and (device-aspect-ratio: 16/9), screen and (device-aspect-ratio: 16/10) {
    * {
        padding: 1em;
    }

    @media (max-width: 200px) and (min-width: 100px) {
        * {
            margin: 1in;
        }
    }
}

@media (10px < width < 1000px) {
  * {
    color: green;
  }
}
</style>
`, 'Verify that media queries are reported properly.');

  const TOTAL_NUMBER_OF_STYLE_SHEETS = 6;
  let numberOfAddedStyleSheets = 0;
  let resolveAllStyleSheetsAdded;
  let isAllStyleSheetsAddedPromiseResolved = false;
  const allStyleSheetsAddedPromise = new Promise(resolve => { resolveAllStyleSheetsAdded = resolve });
  dp.CSS.onStyleSheetAdded(() => {
    numberOfAddedStyleSheets++;

    if (numberOfAddedStyleSheets >= TOTAL_NUMBER_OF_STYLE_SHEETS && !isAllStyleSheetsAddedPromiseResolved)  {
      isAllStyleSheetsAddedPromiseResolved = true;
      resolveAllStyleSheetsAdded();
    }
  });
  await dp.DOM.enable();
  await dp.CSS.enable();

  // Wait until all stylesheets are added for the test
  // otherwise there is a possible race condition in
  // some stylesheets being loading and getMediaQueries
  // request being made.
  await allStyleSheetsAddedPromise;
  var response = await dp.CSS.getMediaQueries();
  var medias = response.result.medias;

  medias.sort((media1, media2) => {
    var compareValue = (value1, value2) => value1 < value2 ? -1 : value1 > value2 ? 1 : 0;
    if (media1.text != media2.text)
      return compareValue(media1.text, media2.text);
    var hasSource1 = !!(media1.styleSheetId && media1.range);
    var hasSource2 = !!(media2.styleSheetId && media2.range);
    if (hasSource1 != hasSource2)
      return compareValue(hasSource1, hasSource2);
    if (media1.range.startLine != media2.range.startLine)
      return compareValue(media1.range.startLine, media2.range.startLine);
    if (media1.range.startColumn != media2.range.startColumn)
      return compareValue(media1.range.startColumn, media2.range.startColumn);
    if (media1.range.endLine != media2.range.endLine)
      return compareValue(media1.range.endLine, media2.range.endLine);
    if (media1.range.endColumn != media2.range.endColumn)
      return compareValue(media1.range.endColumn, media2.range.endColumn);
    return 0;
  });

  var styleSheetIds = new Set(medias.map(media => media.styleSheetId).filter(styleSheetId => !!styleSheetId));
  var styleSheetTexts = new Map();
  for (var styleSheetId of styleSheetIds)
    styleSheetTexts.set(styleSheetId, (await dp.CSS.getStyleSheetText({styleSheetId})).result.text);

  for (var i = 0; i < medias.length; ++i) {
    var mediaRule = medias[i];
    testRunner.log('mediaRule #' + i);
    testRunner.log('    text: ' + mediaRule.text);
    testRunner.log('    source: ' + mediaRule.source);
    // Stabilize keys order in range.
    const range = mediaRule.range && Object.keys(mediaRule.range).sort().reduce(
        (acc, key) => { acc[key] = mediaRule.range[key]; return acc; }, {});
    testRunner.log('    range: ' + JSON.stringify(range));
    if (mediaRule.styleSheetId && mediaRule.range)
      testRunner.log('    computedText: ' + extractText(mediaRule.styleSheetId, mediaRule.range));
    if (!mediaRule.mediaList) {
      testRunner.log('    mediaList is empty');
      continue;
    }
    for (var j = 0; j < mediaRule.mediaList.length; ++j) {
      var mediaQuery = mediaRule.mediaList[j];
      var suffix = mediaRule.sourceURL.indexOf('active-media-queries.css') !== -1 ? ' active: ' + mediaQuery.active : '';
      testRunner.log('    mediaQuery #' + j + suffix);
      for (var k = 0; k < mediaQuery.expressions.length; ++k) {
        var expression = mediaQuery.expressions[k];
        testRunner.log('        mediaExpression #' + k);
        testRunner.log('            feature: ' + expression.feature);
        testRunner.log('            value: ' + expression.value);
        testRunner.log('            unit: ' + expression.unit);
        if (expression.valueRange) {
          testRunner.log('            valueRange: ' + JSON.stringify(expression.valueRange));
          if (mediaRule.styleSheetId)
            testRunner.log('            computedText: ' + extractText(mediaRule.styleSheetId, expression.valueRange));
        }
        if (expression.computedLength)
          testRunner.log('            computed length: ' + expression.computedLength);
      }
    }
  }
  testRunner.completeTest();

  function extractText(styleSheetId, range) {
    var text = styleSheetTexts.get(styleSheetId);
    var lines = text.split('\n');
    var result = [];
    for (var line = range.startLine; line <= range.endLine; ++line) {
      var start = line === range.startLine ? range.startColumn : 0;
      var end = line === range.endLine ? range.endColumn : lines[line].length;
      result.push(lines[line].substring(start, end));
    }
    return result.join('\\n');
  }
});
