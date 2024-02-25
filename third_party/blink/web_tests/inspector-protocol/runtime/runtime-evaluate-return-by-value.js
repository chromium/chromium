(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that Runtime.evaluate returns an error for non serialable types.`);
  testRunner.runTestSuite([
    async function testSymbol() {
      testRunner.log(await dp.Runtime.evaluate({ expression: 'Symbol(239)', returnByValue: true }));
    },

    async function testObjectWithChain() {
      testRunner.log(await dp.Runtime.evaluate({ expression: 'a = {}; a.b = a; a', returnByValue: true }));
    }
  ]);
})
