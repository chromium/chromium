(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  testRunner.log('Tests that multiple sessions observe each other through evaluating.');
  var page = await testRunner.createPage();
  var session1 = await page.createSession();
  var session2 = await page.createSession();
  testRunner.log('window.foo=42 in session1');
  testRunner.log(await session1.protocol.Runtime.evaluate({expression: 'window.foo = 42'}));
  testRunner.log('window.foo in session2');
  testRunner.log(await session2.protocol.Runtime.evaluate({expression: 'window.foo', returnByValue: true}));
  var session3 = await page.createSession();
  testRunner.log('window.foo in session3');
  testRunner.log(await session3.protocol.Runtime.evaluate({expression: 'window.foo', returnByValue: true}));
  testRunner.completeTest();
})
