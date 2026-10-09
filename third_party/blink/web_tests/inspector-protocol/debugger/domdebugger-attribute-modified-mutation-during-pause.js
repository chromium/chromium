(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      '<div id="target" a1="1" a2="2" a3="3" a4="4" a5="5" a6="6" a7="7" t="orig">Target</div>',
      'Tests reallocating element attribute vector while paused on an attribute-modified DOM breakpoint.');

  await dp.Debugger.enable();
  await dp.DOM.enable();

  const {result: doc} = await dp.DOM.getDocument({depth: 10});
  const nodeId = doc.root.children[0].children[1].children[0].nodeId;
  await dp.DOMDebugger.setDOMBreakpoint({nodeId, type: 'attribute-modified'});

  let pausedPromise = dp.Debugger.oncePaused();
  let evalPromise = session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.setAttribute('t', 'updated-after-grow');
    return el.getAttribute('t');
  })()`);
  await pausedPromise;
  await dp.Runtime.evaluate({
    expression: `(() => {
      const el = document.getElementById('target');
      for (let i = 0; i < 32; ++i) {
        el.setAttribute('extra' + i, 'v');
      }
    })()`
  });
  await dp.Debugger.resume();
  testRunner.log('t after grow: ' + await evalPromise);

  pausedPromise = dp.Debugger.oncePaused();
  evalPromise = session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.setAttribute('t', 'updated-after-shrink');
    return el.getAttribute('t');
  })()`);
  await pausedPromise;
  await dp.Runtime.evaluate({
    expression: `(() => {
      const el = document.getElementById('target');
      for (let i = 0; i < 32; ++i) {
        el.removeAttribute('extra' + i);
      }
      for (let i = 1; i <= 7; ++i) {
        el.removeAttribute('a' + i);
      }
    })()`
  });
  await dp.Debugger.resume();
  testRunner.log('t after shrink: ' + await evalPromise);

  pausedPromise = dp.Debugger.oncePaused();
  evalPromise = session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.setAttribute('t', 'updated-after-remove');
    return el.getAttribute('t');
  })()`);
  await pausedPromise;
  await dp.Runtime.evaluate(
      {expression: `document.getElementById('target').removeAttribute('t')`});
  await dp.Debugger.resume();
  testRunner.log('t after remove during set: ' + await evalPromise);

  await dp.DOMDebugger.removeDOMBreakpoint(
      {nodeId, type: 'attribute-modified'});
  await session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.removeAttribute('t');
    for (let i = 1; i <= 4; ++i) {
      el.setAttribute('b' + i, '1');
    }
    el.setAttribute('t', 'to-remove');
  })()`);
  await dp.DOMDebugger.setDOMBreakpoint({nodeId, type: 'attribute-modified'});

  pausedPromise = dp.Debugger.oncePaused();
  evalPromise = session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.removeAttribute('t');
    return el.hasAttribute('t');
  })()`);
  await pausedPromise;
  await dp.Runtime.evaluate({
    expression: `(() => {
      const el = document.getElementById('target');
      for (let i = 1; i <= 4; ++i) {
        el.removeAttribute('b' + i);
      }
    })()`
  });
  await dp.Debugger.resume();
  testRunner.log('has t after shrink during remove: ' + await evalPromise);

  testRunner.completeTest();
})
