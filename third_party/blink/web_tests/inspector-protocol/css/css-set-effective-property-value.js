(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<link rel='stylesheet' href='${testRunner.url('resources/set-active-property-value.css')}'/>
<div id='inspected' style='padding-top: 55px; margin-top: 33px !important; --x:foo'></div>
<div id='append-test' style='padding-left: 10px'/>
  `, 'The test verifies functionality of protocol method CSS.setEffectivePropertyValueForNode.');

  await dp.DOM.enable();
  await dp.CSS.enable();

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  var documentNodeId = await cssHelper.requestDocumentNodeId();

  var nodeId = await cssHelper.requestNodeId(documentNodeId, '#inspected');

  async function updateProperty(propertyName, value) {
    await dp.CSS.setEffectivePropertyValueForNode({nodeId, propertyName, value});
    await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#inspected', true /* omitLog */);
    await dp.DOM.undo();
  };

  await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#inspected');

  testRunner.runTestSuite([
    async function testBasicPropertyChange() {
      await updateProperty('padding-left', '101px');
    },

    async function testChangePropertyInShortHand() {
      await updateProperty('padding-bottom', '101px');
    },

    async function testChangeImportantProperty() {
      await updateProperty('margin-left', '101px');
    },

    async function testChangeInlineProperty() {
      await updateProperty('padding-top', '101px');
    },

    async function testChangeInlineImportantProperty() {
      await updateProperty('margin-top', '101px');
    },

    async function testChangeMissingProperty() {
      await updateProperty('margin-bottom', '101px');
    },

    async function testAppendWithSeparator() {
      await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#append-test');
      var nodeId = await cssHelper.requestNodeId(documentNodeId, '#append-test');

      testRunner.log('Resulting styles');
      await dp.CSS.setEffectivePropertyValueForNode({nodeId, propertyName: 'padding-right', value : '101px'});
      await cssHelper.loadAndDumpInlineAndMatchingRules(documentNodeId, '#append-test', true /* omitLog */);
    },

    async function testChangeCustomProperty() {
      await updateProperty('--x', 'bar');
    }

  ]);
});

