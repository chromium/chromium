(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
      `
  <style>
  div {
    --prop: red;
    --second-prop: red;
    animation: 3s keyframe;
  }

  @property --prop {
    syntax: "<number>";
    inherits: false;
    initial-value: 5;
  }

  @keyframes keyframe {
    0% { left: 0; }
    100% { left: 0; }
  }
  </style>

  <div>div</div>
  `,
      'Test that the property name of property rules is editable');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: 'div'});
  {
    const {result: {computedStyle}} =
        await dp.CSS.getComputedStyleForNode({nodeId});
    testRunner.log('Original values:');
    testRunner.log(computedStyle.find(style => style.name === '--prop'));
    testRunner.log(computedStyle.find(style => style.name === '--second-prop'));
  }

  {
    const {
      result: {cssPropertyRules: [{propertyName: {range}, styleSheetId}]}
    } = await dp.CSS.getMatchedStylesForNode({nodeId});

    const result = await dp.CSS.setPropertyRulePropertyName(
        {styleSheetId, range, propertyName: '--second-prop'});
    testRunner.log('Set property name:');
    testRunner.log(result);
  }
  {
    const {result: {computedStyle}} =
        await dp.CSS.getComputedStyleForNode({nodeId});
    testRunner.log('Original values:');
    testRunner.log(computedStyle.find(style => style.name === '--prop'));
    testRunner.log(computedStyle.find(style => style.name === '--second-prop'));
  }

  {
    const {
      result: {cssPropertyRules: [{propertyName: {range}, styleSheetId}]}
    } = await dp.CSS.getMatchedStylesForNode({nodeId});

    const result = await dp.CSS.setPropertyRulePropertyName(
        {styleSheetId, range, propertyName: '--prop'});
    testRunner.log('Revert property name:');
    testRunner.log(result);
  }
  {
    const {result: {computedStyle}} =
        await dp.CSS.getComputedStyleForNode({nodeId});
    testRunner.log('Original values:');
    testRunner.log(computedStyle.find(style => style.name === '--prop'));
    testRunner.log(computedStyle.find(style => style.name === '--second-prop'));
  }

  const {
    result: {
      cssKeyframesRules,
      cssPropertyRules: [{propertyName: {range}, styleSheetId}]
    }
  } = await dp.CSS.getMatchedStylesForNode({nodeId});

  {
    const {error: {message}} = await dp.CSS.setPropertyRulePropertyName(
        {styleSheetId, range, propertyName: 'prop'});
    testRunner.log(`Expected error: ${message}`);
  }

  {
    const {error: {message}} = await dp.CSS.setPropertyRulePropertyName({
      styleSheetId,
      range: {startLine: 0, startColumn: 0, endLine: 0, endColumn: 0},
      propertyName: '--prop'
    });
    testRunner.log(`Expected error: ${message}`);
  }

  {
    const someOtherRange = cssKeyframesRules[0].keyframes[0].keyText.range;
    const {error: {message}} = await dp.CSS.setPropertyRulePropertyName(
        {styleSheetId, range: someOtherRange, propertyName: '--prop'});
    testRunner.log(`Expected error: ${message}`);
  }

  testRunner.completeTest();
});
