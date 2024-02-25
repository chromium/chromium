(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that property defined on console.__proto__ doesn't observable on other Objects.`);
  testRunner.log(await dp.Runtime.evaluate({expression: `
    var amountOfProperties = 0;
    for (var p in {})
      ++amountOfProperties;
    console.__proto__.debug = 239;
    for (var p in {})
      --amountOfProperties;
    amountOfProperties
  `}));
  testRunner.completeTest();
})
