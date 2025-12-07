(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  const relativeLengthUnits = ["em", "ex", "cap", "ch", "ic", "lh", "rcap", "rch", "rem", "rex", "ric", "rlh", "vw", "vh ", "vi", "vb", "vmin", "vmax"];
  const lengthExpressions = ["calc(1em + 10px)", "calc(1em + 3em)", "calc(3px + 2.54cm)", "clamp(10px, calc(10rem + 10rex), 30px)", "max(100px, 30em)", "calc(100px * cos(60deg))"];
  const invalidLengthValues = ["calc(", "em", "calc(10 + 20)", "red", "calc(10ms + 5s)"];
  const validNumberValues = ["100", "log(1000, 10)", "calc(10 + 30)"];
  const testValues = ["invalid", "1em", "1rem", "calc(3px + 3px)", "calc(1em + 1px)"];
  const cssWideKeywords = ["initial", "inherit", "unset"];
  const validColorValues = ["aqua", "peachpuff", "blanchedalmond", "rgb(255, 0, 0)", "#0f5ffe", "color-mix(in srgb, plum, #f00)"];
  const validPercentageExpressions = ["10%", "50%", "calc(10% + 10%)", "calc(10% - 10%)", "calc(10px + 10% - 10%)", "calc(10px + 10px)", "calc(10px + 0%)", "calc(10px + 10%)", "calc(1em + 10%)"];
  const invalidPercentageExpressions = ["calc(", "%", "calc(10 + 20)%", "calc(10 + 30%"];
  const arbSubs = ["var(--x)", "attr(data-foo type(<length>))", "attr(invalid, 3px)", "var(--invalid, 3px)", "var(--cycle1)"];

  var {page, session, dp} = await testRunner.startURL('resources/css-resolve-values.html', 'Test css.resolveValue method');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  function print(inputValues, response) {
    testRunner.log("{\"input\": [" + inputValues.toString() + "]}");
    if (response.error) {
      testRunner.log(JSON.stringify(response.error));
    } else {
      testRunner.log(JSON.stringify(response.result));
    }
  }

  async function testResolveValues(querySelector, values, propertyName) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId, propertyName: propertyName});
    print(values, response);
  }

  async function testResolveValuesWithoutProperty(querySelector, values) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId});
    print(values, response);
  }

  async function testResolveValuesForPseudoElement(querySelector, values, propertyName, pseudoType) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId, propertyName: propertyName, pseudoType: pseudoType});
    print(values, response);
  }

  async function testResolveValuesForPseudoElementWithoutProperty(querySelector, values, pseudoType) {
    var nodeId = await cssHelper.requestNodeId(documentNodeId, querySelector);
    var response = await dp.CSS.resolveValues({values: values, nodeId: nodeId, pseudoType: pseudoType});
    print(values, response);
  }

  testRunner.runTestSuite([
    async function testNotExistentNode() {
      testRunner.log('Test non existent node');
      await testResolveValues('.notexistent', testValues, "height");
    },
    async function testInvalidProperty() {
      testRunner.log('Test invalid property name');
      await testResolveValues('.outer', testValues, "invalid");
    },
    async function testShorthandProperty() {
      testRunner.log('Test shorthand property');
      await testResolveValues('.inner', lengthExpressions, "margin");
    },
    async function testResolveValuesSimple() {
      testRunner.log('Test resolveValues for width property');
      await testResolveValues('.inner', testValues, "width");
    },
    async function testResolveValuesSimpleNoProperty() {
      testRunner.log('Test resolveValues no property specified');
      await testResolveValuesWithoutProperty('.inner', testValues);
    },
    async function testRelativeUnits() {
      testRunner.log('Relative length units to absolute test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValues('.inner', relativeValues, "width");
    },
    async function testResolveValuesRelativeUnitsNoProperty() {
      testRunner.log('Test resolveValues on relative units no property specified');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValuesWithoutProperty('.inner', relativeValues);
    },
    async function testRelativeUnitsNoProperty() {
      testRunner.log('Relative length units to absolute no property specified test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValuesWithoutProperty('.inner', relativeValues);
    },
    async function testRelativeUnitsOuter() {
      testRunner.log('Relative length units to absolute for parent element test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValues('.outer', relativeValues, "width");
    },
    async function testRelativeUnitsOuterNoProperty() {
      testRunner.log('Relative length units to absolute for parent element no property specified test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValuesWithoutProperty('.outer', relativeValues);
    },
    async function testRelativeUnitsFontSize() {
      testRunner.log('Relative length units to absolute font-size property test');
      var relativeValues = relativeLengthUnits.map((x) => "1" + x);
      await testResolveValues('.inner', relativeValues, "font-size");
    },
    async function testExpressionsEvaluation() {
      testRunner.log('Evaluate expression test');
      await testResolveValues('.inner', lengthExpressions, "height");
    },
    async function testExpressionsEvaluationNoProperty() {
      testRunner.log('Evaluate expression with no property specified test');
      await testResolveValuesWithoutProperty('.inner', lengthExpressions);
    },
    async function testCSSWideKeywords() {
      testRunner.log('Test CSS-wide keywords');
      await testResolveValues('.inner', cssWideKeywords, "font-size");
    },
    async function testCSSWideKeywordsNoProperty() {
      testRunner.log('Test CSS-wide keywords with no property specified');
      await testResolveValues('.inner', cssWideKeywords);
    },
    async function testColorValues() {
      testRunner.log('Test <color> values');
      await testResolveValues('.inner', validColorValues, "background-color");
    },
    async function testColorValuesNoProperty() {
      testRunner.log('Test <color> values with no property specified');
      await testResolveValuesWithoutProperty('.inner', validColorValues);
    },
    async function testInvalidLengthValues() {
      testRunner.log('Invalid length values test');
      await testResolveValues('.inner', invalidLengthValues, "width");
    },
    async function testCustomProperty() {
      testRunner.log('Test resolveValues on custom property');
      await testResolveValues('div', testValues, "--prop");
    },
    async function testCustomProperty() {
      testRunner.log('Test resolveValues on custom property with cycle should ignore custom property');
      await testResolveValues('div', ["var(--prop)"], "--prop");
    },
    async function testRegisterCustomProperty() {
      testRunner.log('Test resolveValues on register custom property');
      await testResolveValues('div', testValues, "--reg-prop");
    },
    async function testPseudoElement() {
      testRunner.log('Pseudo element test');
      await testResolveValuesForPseudoElement('div', testValues, "height", 'after');
    },
    async function testPseudoElementNoProperty() {
      testRunner.log('Pseudo element with no property specified test');
      await testResolveValuesForPseudoElementWithoutProperty('div', testValues, 'after');
    },
    async function testNonExistentPseudoElement() {
      testRunner.log('Non existent pseudo element test');
      await testResolveValuesForPseudoElement('div', testValues, "height", 'marker');
    },
    async function testPseudoElementsNoElement() {
      testRunner.log('Pseudo element with no content');
      await testResolveValuesForPseudoElement('div', testValues, "width", 'before');
    },
    async function testElementDisplayNone() {
      testRunner.log('Test on element without computed style');
      await testResolveValues('.display-none', testValues, "height");
    },
    async function testResolveValuesIgnoreProperty() {
      testRunner.log('Test resolveValues should ignore property');
      await testResolveValues('.inner', validNumberValues, "width");
    },
    async function testResolveValuesValidInnerHeight() {
      testRunner.log('Test resolveValues for height property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "height");
    },
    async function testResolveValuesValidOuterWidth() {
      testRunner.log('Test resolveValues for width property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "width");
    },
    async function testResolveValuesValidInnerMinHeight() {
      testRunner.log('Test resolveValues for min-height property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "min-height");
    },
    async function testResolveValuesValidOuterMinWidth() {
      testRunner.log('Test resolveValues for min-width property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "min-width");
    },
    async function testResolveValuesValidInnerMaxWidth() {
      testRunner.log('Test resolveValues for max-width property of outer element');
      await testResolveValues('.inner', validPercentageExpressions, "max-width");
    },
    async function testResolveValuesValidOuterMaxHeight() {
      testRunner.log('Test resolveValues for max-height property of inner element');
      await testResolveValues('.outer', validPercentageExpressions, "max-height");
    },
    async function testResolveValuesValidInnerLeft() {
      testRunner.log('Test resolveValues for left property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "left");
    },
    async function testResolveValuesValidInnerTop() {
      testRunner.log('Test resolveValues for top property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "top");
    },
    async function testResolveValuesValidOuterRight() {
      testRunner.log('Test resolveValues for right property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "right");
    },
    async function testResolveValuesValidOuterBottom() {
      testRunner.log('Test resolveValues for bottom property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "bottom");
    },
    async function testResolveValuesValidInnerMarginLeft() {
      testRunner.log('Test resolveValues for margin-left property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "margin-left");
    },
    async function testResolveValuesValidInnerMarginBottom() {
      testRunner.log('Test resolveValues for margin-bottom property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "margin-bottom");
    },
    async function testResolveValuesValidOuterMarginTop() {
      testRunner.log('Test resolveValues for margin-top property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "margin-top");
    },
    async function testResolveValuesValidOuterMarginRight() {
      testRunner.log('Test resolveValues for margin-right property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "margin-right");
    },
    async function testResolveValuesValidInnerPaddingLeft() {
      testRunner.log('Test resolveValues for padding-left property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "padding-left");
    },
    async function testResolveValuesValidInnerPAddingTop() {
      testRunner.log('Test resolveValues for padding-top property of inner element');
      await testResolveValues('.inner', validPercentageExpressions, "padding-top");
    },
    async function testResolveValuesValidOuterPaddingRight() {
      testRunner.log('Test resolveValues for padding-right property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "padding-right");
    },
    async function testResolveValuesValidOuterPaddingBottom() {
      testRunner.log('Test resolveValues for padding-bottom property of outer element');
      await testResolveValues('.outer', validPercentageExpressions, "padding-bottom");
    },
    async function testResolvePercentageValuesNoProperty() {
      testRunner.log('Test resolveValues with invalid percentage expressions');
      await testResolveValues('.inner', validPercentageExpressions);
    },
    async function testResolveInvalidPercentageValues() {
      testRunner.log('Test resolveValues with invalid percentage expressions');
      await testResolveValues('.inner', invalidPercentageExpressions, "height");
    },
    async function testResolveValuesWithVar() {
      testRunner.log('Test resolveValues with var() for width property');
      await testResolveValues('.inner', arbSubs, "width");
    }
  ]);
});