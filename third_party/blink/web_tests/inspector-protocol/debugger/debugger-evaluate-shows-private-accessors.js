(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that the inspector can inspect private properties');

  await dp.Debugger.enable();

  const {result} = await dp.Runtime.evaluate({
    expression: `
      (() => {
        class A {
          get #bar() { return 42; }
        };
        return new A();
      })()`,
  });

  testRunner.log(result);
  const objectId = result.result.objectId;

  const {result: {privateProperties}} =
      await dp.Runtime.getProperties({objectId});
  testRunner.log(privateProperties);

  const name = privateProperties[0].name;
  const getter = privateProperties[0].get;

  const call = await dp.Runtime.callFunctionOn({functionDeclaration: `${getProperty}`, objectId, arguments: [{objectId: getter.objectId}]});
  testRunner.log(call);

  testRunner.completeTest();

  function getProperty(getter) {
    return Reflect.apply(getter, this, []);
  }
});
