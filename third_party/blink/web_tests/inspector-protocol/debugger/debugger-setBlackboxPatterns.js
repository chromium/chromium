(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests setBlackboxPatterns functionality.');

  await session.evaluate(`
    function bar()
    {
        return 42;
    }
  `);

  await session.evaluate(`
    function foo()
    {
        var a = bar();
        return a + 1;
    }
    //# sourceURL=foo.js
  `);

  await session.evaluate(`
    function qwe()
    {
        var a = foo();
        return a + 1;
    }
    //# sourceURL=qwe.js
  `);

  await session.evaluate(`
    function baz()
    {
        var a = qwe();
        return a + 1;
    }
    //# sourceURL=baz.js
  `);

  function logStack(message) {
    testRunner.log('Paused in');
    var callFrames = message.params.callFrames;
    for (var callFrame of callFrames)
      testRunner.log((callFrame.functionName || '(...)') + ':' + (callFrame.location.lineNumber + 1));
  }

  dp.Debugger.enable();
  var message = await dp.Debugger.setBlackboxPatterns({patterns: ['foo([']});
  testRunner.log(message.error.message);

  dp.Debugger.setBlackboxPatterns({patterns: ['baz\.js', 'foo\.js']});
  dp.Runtime.evaluate({expression: 'debugger;baz()' });
  logStack(await dp.Debugger.oncePaused());

  dp.Debugger.stepInto();
  logStack(await dp.Debugger.oncePaused());

  dp.Debugger.stepInto();
  logStack(await dp.Debugger.oncePaused());

  dp.Debugger.stepInto();
  logStack(await dp.Debugger.oncePaused());

  dp.Debugger.stepOut();
  logStack(await dp.Debugger.oncePaused());

  dp.Debugger.stepInto();
  logStack(await dp.Debugger.oncePaused());

  dp.Debugger.stepInto();
  logStack(await dp.Debugger.oncePaused());

  await dp.Debugger.resume();
  testRunner.completeTest();
})

