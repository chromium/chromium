(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that Command Line API doesn't override defined on window methods and can be overridden during evaluation.`);

  await session.evaluate(`
    function overrideDir() {
      var v = '' + dir;
      dir = 239;
      return v + ' -> ' + dir;
    }

    function override$_() {
      var v = '' + $_;
      $_ = 239;
      return v + ' -> ' + $_;
    }

    function doesCommandLineAPIEnumerable() {
      for (var v in window) {
        if (v === 'dir' || v === '$_')
          return 'enumerable';
      }
      return 'non enumerable';
    }
  `);

  async function evaluate(expression) {
    var result = await dp.Runtime.evaluate({ 'expression': expression, objectGroup: 'console', includeCommandLineAPI: true });
    return result.result;
  }

  function dumpResult(title, message) {
    testRunner.log(title);
    testRunner.log(message.result.value);
  }

  dumpResult(`Check that CommandLineAPI isn't enumerable on window object:`,
      await evaluate(`doesCommandLineAPIEnumerable()`));
  dumpResult(`Override dir:`,
      await evaluate(`overrideDir()`));
  dumpResult(`CommandLineAPI doesn't override dir:`,
      await evaluate(`'' + dir`));
  await evaluate(`delete dir`);
  dumpResult(`CommandLineAPI is presented after removing override variable:`,
      await evaluate(`overrideDir()`));
  // set $_ to 42
  await evaluate(`42`);
  dumpResult(`Override $_:`,
      await evaluate(`override$_()`));
  dumpResult(`CommandLineAPI doesn't override $_:`,
      await evaluate(`'' + $_`));
  await evaluate(`delete $_`);
  dumpResult(`CommandLineAPI is presented after removing override variable:`,
      await evaluate(`override$_()`));
  dumpResult(`CommandLineAPI can be overridden by var dir = 1:`,
      await evaluate(`var dir = 1; '' + dir`));
  dumpResult(`CommandLineAPI doesn't override var dir = 1:`,
      await evaluate(`'' + dir`));
  dumpResult(`CommandLineAPI can be overridden by Object.defineProperty:`,
      await evaluate(`Object.defineProperty(window, 'copy', { get: () => 239 }); '' + copy`));
  dumpResult(`CommandLineAPI doesn't override Object.defineProperty:`,
      await evaluate(`'' + copy`));
  testRunner.completeTest();
})
