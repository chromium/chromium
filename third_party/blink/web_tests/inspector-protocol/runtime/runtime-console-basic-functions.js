(async function testRemoteObjects(testRunner) {
  const {dp, session} = await testRunner.startBlank('Tests ConsoleOM APIs with standard argument behavior.');
  dp.Runtime.enable();
  const response1 = await dp.Runtime.onceExecutionContextCreated();
  const pageContextId = response1.params.context.id; // main page
  session.evaluate(`
    window.frame = document.createElement('iframe');
    frame.src = '${testRunner.url('../resources/blank.html')}';
    document.body.appendChild(frame);
  `);
  const response2 = await dp.Runtime.onceExecutionContextCreated();
  const frameContextId = response2.params.context.id; // IFrame
  const console_argsRequired = [
    'log',
    'debug',
    'info',
    'error',
    'warn',
    'dir',
    'dirxml',
    'table'
  ];
  const console_argsOptional = [
    'trace',
    'clear',
    'group',
    'groupCollapsed',
    'groupEnd'
  ];
  const configs = [
    pageContextId,
    frameContextId
  ];

  dp.Runtime.onConsoleAPICalled(result => testRunner.log(result));

  for (const contextId of configs) {
    for (const func of console_argsRequired) {
      logConsoleTestMethod(func, true, contextId);
      await dp.Runtime.evaluate({ expression: `console.${func}({a:3, b:"hello"})`, contextId });
    }

    for (const func of console_argsOptional) {
      logConsoleTestMethod(func, false, contextId);
      await dp.Runtime.evaluate({ expression: `console.${func}()`, contextId });
    }
  }

  testRunner.completeTest();

  function logConsoleTestMethod(func, required, contextId) {
    const context = getContextType(contextId);
    const contextString = context ? `inside ${context} context` : '';
    const argType = required ? 'required' : 'optional';
    testRunner.log(`Testing console.${func} with ${argType} args ${contextString}`);
  }

  function getContextType(contextId) {
    if (contextId === pageContextId) {
      return 'page';
    } else if (contextId === frameContextId) {
      return 'frame';
    }
  }
});