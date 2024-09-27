(async function test(testRunner) {
  var {session, dp} = await testRunner.startHTML(`
    <!DOCTYPE html>
    <html>
    <meta charset="UTF-8">
    <head>
        <!-- available fonts --->
        <style type="text/css">
        </style>
        <link rel="stylesheet" type="text/css" href="${testRunner.url("resources/applied-styles.css")}">
    </head>
    <body>
        <div class="test">
            <div id="condensed_normal_100">abcdefg</div>
            <div id="condensed_normal_900">abcdefg</div>
            <div id="condensed_italic_100">abcdefg</div>
            <div id="condensed_italic_900">abcdefg</div>
            <div id="expanded_normal_100">abcdefg</div>
            <div id="expanded_normal_900">abcdefg</div>
            <div id="expanded_italic_100">abcdefg</div>
            <div id="expanded_italic_900">abcdefg</div>
        </div>
    </body>
    </html>
  `, `According to the CSS3 Fonts Module, Step 4 or the Font Style Matching Algorithm
( https://drafts.csswg.org/css-fonts-3/#font-style-matching ) must narrow down the
available font faces by finding the nearest match in the following order of
precedence: stretch, style, weight.`);

  await dp.DOM.enable();
  await dp.CSS.enable();

  await session.evaluate(`
    function updateFont(available) {
      var fulfill;
      var promise = new Promise(f => fulfill = f);

      function emptyFontFaceDeclarations()
      {
          var stylesheet = document.styleSheets[0];
          var rules = stylesheet.cssRules;
          var rulesLengthSnapshot = stylesheet.cssRules.length;
          if (!rulesLengthSnapshot)
              return;
          for (var i = 0; i < rulesLengthSnapshot; i++) {
              stylesheet.deleteRule(0);
          }
      }

      function makeFontFaceDeclaration(stretch, style, weight)
      {
          var fontFaceDeclaration = '@font-face { font-family: "CSSMatchingtest"; ' +
              "font-stretch: " + stretch + ";" +
              "font-style: " + style + ";" +
              "font-weight: " + weight + ";" +
              'src: url("' +
              "${testRunner.url("resources/fonts/CSSMatchingTest_")}" +
              stretch + "_" + style + "_" + weight + '.ttf");';
          return fontFaceDeclaration;
      }

      function notifyInspectorFontsReady() {
          var cssRules = document.styleSheets[0].cssRules;
          var fontsAvailable = [];
          for (var i = 0; i < cssRules.length; i++) {
              urlmatch = \/url\\(".*fonts\\/CSSMatchingTest_(.*).ttf"\\)/.exec(
                  cssRules[i].cssText);
              fontsAvailable.push(urlmatch[1]);
          }
          fulfill(JSON.stringify(fontsAvailable));
      }

      function updateAvailableFontFaceDeclarations(available)
      {
          emptyFontFaceDeclarations();
          for (stretch of available.stretches)
              for (style of available.styles)
                  for (weight of available.weights) {
                      document.styleSheets[0].addRule(
                          makeFontFaceDeclaration(stretch, style, weight));
                  }

          document.fonts.ready.then(() => {
              // fonts.ready event fires too early, used fonts for rendering have not
              // been updated yet. Force a layout to hopefully work around this.
              // Remove this when crbug.com/516680 is fixed.
              // https://drafts.csswg.org/css-font-loading/#font-face-set-ready
              document.body.offsetTop;
              notifyInspectorFontsReady();
          });
      }
      updateAvailableFontFaceDeclarations(available);
      return promise;
    }
  `);

  var documentNodeId;
  var documentNodeSelector;
  var allTestSelectors = [];
  var testSelectors = [];
  var testGroup = 0;

  var stretches = ['condensed', 'expanded'];
  var styles = ['normal', 'italic'];
  var weights = ['100', '900'];

  var weightsHumanReadable = ["Thin", "Black"];

  var fontSetVariations = [];
  var currentFontSetVariation = {};

  makeFontSetVariations();

  function makeFontSetVariations() {
    // For each of the three properties we have three choices:
    // Restrict to the first value, to the second, or
    // allow both. So we're iterating over those options
    // for each dimension to generate the set of allowed
    // options for each property. The fonts in the test
    // page will later be generated from these possible
    // choices / restrictions.
    function choices(property) {
      return [property, [property[0]], [property[1]]];
    }

    for (var stretchChoice of choices(stretches)) {
      for (var styleChoice of choices(styles)) {
        for (var weightChoice of choices(weights)) {
          var available = {};
          available.stretches = stretchChoice;
          available.styles = styleChoice;
          available.weights = weightChoice;
          fontSetVariations.push(available);
        }
      }
    }
  }

  function subsetFontSetVariations() {
    var NUM_GROUPS = 9;
    var numVariations = fontSetVariations.length;
    var groupLength = numVariations / NUM_GROUPS;
    var start = testGroup * groupLength;
    var end = start + groupLength;
    testRunner.log("Testing font set variations " + start + " to " +
      (end - 1) + " out of 0-" + (numVariations - 1));
    fontSetVariations = fontSetVariations.slice(start, end);
  }

  dp.DOM.getDocument({}).then(({ result }) => onDocumentNodeId(result.root.nodeId));

  function onDocumentNodeId(nodeId) {
    documentNodeId = nodeId;
    var params = new URLSearchParams(window.location.search);
    var testURL = params.get('test');
    testGroup = /font-style-matching-(\d).js/.exec(
      testURL)[1];
    subsetFontSetVariations();
    session.evaluate(
      'JSON.stringify(' +
      'Array.prototype.map.call(' +
      'document.querySelectorAll(".test *"),' +
      'function(element) { return element.id } ));',
    ).then(startTestingPage);
  }

  function startTestingPage(result) {
    allTestSelectors = JSON.parse(result);
    nextTest();
  }

  function nextTest() {
    if (testSelectors.length) {
      testNextPageElement();
    } else {
      currentFontSetVariation = fontSetVariations.shift();
      if (currentFontSetVariation) {
        testSelectors = allTestSelectors.slice();
        updateFontDeclarationsInPageForVariation(
          currentFontSetVariation);
      } else {
        testRunner.completeTest();
      }
    }
  }

  async function updateFontDeclarationsInPageForVariation(fontSetVariation) {
    var loadedFonts = await session.evaluateAsync(
      'updateFont(' +
      JSON.stringify(fontSetVariation) +
      ');');
    testRunner.log("Available fonts updated: " + loadedFonts + "\n");
    nextTest();
  }

  function testNextPageElement(result) {
    var nextSelector = testSelectors.shift()
    if (nextSelector) {
      documentNodeSelector = "#" + nextSelector;
      platformFontsForElementWithSelector(documentNodeSelector);
    }
  }

  async function platformFontsForElementWithSelector(selector) {
    var response = await dp.DOM.querySelector({ nodeId: documentNodeId, selector });
    var nodeId = response.result.nodeId;
    var response = await dp.CSS.getPlatformFontsForNode({ nodeId });
    logResults(response);
    nextTest();
  }

  function logResults(response) {
    testRunner.log(documentNodeSelector);
    logPassFailResult(
      documentNodeSelector,
      response.result.fonts[0].familyName);
  }

  function cssStyleMatchingExpectationForSelector(selector) {
    var selectorStretchStyleWeight = selector.substr(1).split("_");
    var selectorStretch = selectorStretchStyleWeight[0].toLowerCase();
    var selectorStyle = selectorStretchStyleWeight[1].toLowerCase();
    var selectorWeight = selectorStretchStyleWeight[2];

    var expectedProperties = {};
    if (currentFontSetVariation.stretches.indexOf(selectorStretch) > 0) {
      expectedProperties.stretch = selectorStretch;
    } else {
      // If the requested property value is not availabe in the
      // current font set, then it's restricted to only one value,
      // which is the nearest match, and at index 0.
      expectedProperties.stretch = currentFontSetVariation.stretches[0];
    }

    if (currentFontSetVariation.styles.indexOf(selectorStyle) > 0) {
      expectedProperties.style = selectorStyle;
    } else {
      expectedProperties.style = currentFontSetVariation.styles[0];
    }

    if (currentFontSetVariation.weights.indexOf(selectorWeight) > 0) {
      expectedProperties.weight = selectorWeight;
    } else {
      expectedProperties.weight = currentFontSetVariation.weights[0];
    }

    return expectedProperties;
  }

  function logPassFailResult(selector, usedFontName) {
    var actualStretchStyleWeight =
      /CSSMatchingTest (\w*) (\w*) (\w*)/.exec(usedFontName);
    var actualStretch, actualStyle, actualWeight;
    if (actualStretchStyleWeight && actualStretchStyleWeight.length > 3) {
      actualStretch = actualStretchStyleWeight[1].toLowerCase();
      actualStyle = actualStretchStyleWeight[2].toLowerCase();
      // Font names have human readable weight description,
      // we need to convert.
      actualWeight = weights[
        weightsHumanReadable.indexOf(actualStretchStyleWeight[3])];
    } else {
      actualStretch = usedFontName;
      actualStyle = "";
      actualWeight = "";
    }

    var expectedProperties = cssStyleMatchingExpectationForSelector(
      selector);

    testRunner.log("Expected: " +
      expectedProperties.stretch + " " +
      expectedProperties.style + " " +
      expectedProperties.weight);
    testRunner.log("Actual: " + actualStretch + " " +
      actualStyle + " " +
      actualWeight);

    if (actualStretch != expectedProperties.stretch ||
      actualStyle != expectedProperties.style ||
      actualWeight != expectedProperties.weight) {
      testRunner.log("FAIL\n");
    } else {
      testRunner.log("PASS\n");
    }
  }
})
