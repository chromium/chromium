(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  var {page, session, dp} = await testRunner.startHTML(`<div></div>`, 'Test css.getLonghandProperties method');

  await dp.DOM.enable();
  await dp.CSS.enable();

  function print(shorthandName, value, response) {
    testRunner.log(shorthandName + ": " + value);
    testRunner.log("Resolved longands:");
    if (response.error) {
      testRunner.log(JSON.stringify(response.error));
    } else {
      response.result.longhandProperties.forEach((longhandProperty) => {
        testRunner.log(longhandProperty["name"] + ": " + longhandProperty["value"]);
      });
    }
  }

  async function testGetLonghandProperties(value, shorthandName) {
    var response = await dp.CSS.getLonghandProperties({shorthandName: shorthandName, value: value});
    print(shorthandName, value, response);
  }

  testRunner.runTestSuite([
    async function testInvalidProperty() {
      testRunner.log('Test getLonghandPropertiesFont for non-existing property');
      await testGetLonghandProperties('10px 50px 20px 0', 'marg');
    },
    async function testInvalidValue() {
      testRunner.log('Test getLonghandPropertiesFont for invalid value');
      await testGetLonghandProperties('italic small cursive', 'font');
    },
    async function testLonghandValue() {
      testRunner.log('Test getLonghandPropertiesFont for longhand property');
      await testGetLonghandProperties('italic', 'font-style');
    },
    async function testFontProperty() {
      testRunner.log('Test getLonghandPropertiesFont for font property');
      await testGetLonghandProperties('italic small-caps bold 16px/2 cursive', 'font');
    },
    async function testMarginProperty() {
      testRunner.log('Test getLonghandPropertiesFont for margin property');
      await testGetLonghandProperties('10px 50px 20px 0', 'margin');
    },
    async function testBackgroundProperty() {
      testRunner.log('Test getLonghandPropertiesFont for background property');
      await testGetLonghandProperties('center / contain no-repeat url("../../media/examples/firefox-logo.svg"), #eee 35% url("../../media/examples/lizard.png")', 'background');
    },
  ]);
});