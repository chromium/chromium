(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var { page, session, dp } = await testRunner.startHTML(`
    <div id='divUnderTest'>Test</div>
  `, `Tests set DOM breakpoint.`);

  await dp.Debugger.enable();
  await dp.DOM.enable();
  await dp.DOMDebugger.enable();

  const { result: document } = await dp.DOM.getDocument({ depth: 10 });
  const nodeId = document.root.children[0].children[1].children[0].nodeId;

  async function testDOMBreakpoint(type, expression) {
    testRunner.log('Testing DOM breakpoint for ' + type);
    await dp.DOMDebugger.setDOMBreakpoint({ nodeId, type });
    const promise = dp.Debugger.oncePaused();
    // Do not await here because the evaluate call will be stuck in debugger paused otherwise.
    dp.Runtime.evaluate({ expression });
    const messageObject = await promise;
    await dp.Debugger.resume();
    testRunner.log('Debugger paused on ' + messageObject.params.data.type);
    await dp.DOMDebugger.removeDOMBreakpoint({ nodeId, type });
  }


  await testDOMBreakpoint('attribute-modified', `
    document.getElementById('divUnderTest').setAttribute('test', 'Hello World');
  `);

  await testDOMBreakpoint('subtree-modified', `
    document.getElementById('divUnderTest').childNodes[0].nodeValue = 'New node value';
  `);

  await testDOMBreakpoint('subtree-modified', `
    const child = document.createElement('span');
    child.textContent = 'Hello World';
    document.getElementById('divUnderTest').appendChild(child);
  `);

  await testDOMBreakpoint('node-removed', `
    document.getElementById('divUnderTest').remove();
  `);

  testRunner.completeTest();
})
