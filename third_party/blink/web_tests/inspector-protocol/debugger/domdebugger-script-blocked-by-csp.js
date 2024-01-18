(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <html>
    <head>
    <meta http-equiv='Content-Security-Policy' content="script-src 'self';">
    </head>
    <body>
    <button id='testButton' onclick='alert(1);'>Button</button>
    </body>
    </html>
  `, `Tests pausing on scriptBlockedbyCSP breakpoint.`);

  dp.Debugger.enable();
  dp.DOM.enable();
  dp.DOMDebugger.enable();
  dp.DOMDebugger.setInstrumentationBreakpoint({eventName: 'scriptBlockedByCSP'});

  var expressions = [
    `
    document.getElementById('testButton').click();
    `,

    `
    var script = document.createElement('script');
    script.innerText = 'alert(1)';
    document.body.appendChild(script);
    `,

    `
    var a = document.createElement('a');
    a.setAttribute('href', 'javascript:alert(1);');
    var dummy = 1;
    document.body.appendChild(a); a.click();
    `
  ];
  var descriptions = [
    'blockedEventHandler',
    'blockedScriptInjection',
    'blockedScriptUrl'
  ];

  for (var i = 0; i < expressions.length; i++) {
    var funcName = descriptions[i];
    testRunner.log('\n-------\n' + funcName);
    dp.Runtime.evaluate({expression: 'function ' + funcName + '() {' + expressions[i] + '}\n' + funcName + '()'});
    var messageObject = await dp.Debugger.oncePaused();
    var params = messageObject.params;
    testRunner.log('Paused at: ' + params.callFrames[0].functionName + '@' + params.callFrames[0].location.lineNumber);
    testRunner.log('Reason: ' + params.reason + '; Data:');
    testRunner.log(params.data);
    await dp.Debugger.resume();
  }

  testRunner.completeTest();
})
