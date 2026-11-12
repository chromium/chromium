(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      '<div id="target" a1="1" a2="2" a3="3">Target</div>',
      'Tests mutating element attribute vector while paused on an attribute-modified DOM breakpoint during attribute append.');

  await dp.Debugger.enable();
  await dp.DOM.enable();

  const {result: doc} = await dp.DOM.getDocument({depth: 10});
  const nodeId = doc.root.children[0].children[1].children[0].nodeId;
  await dp.DOMDebugger.setDOMBreakpoint({nodeId, type: 'attribute-modified'});

  // 1. Grow attribute vector during pause in AppendAttributeInternal.
  let pausedPromise = dp.Debugger.oncePaused();
  let evalPromise = session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.setAttribute('new-attr-grow', 'val-grow');
    return el.getAttribute('new-attr-grow');
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
  testRunner.log('new-attr-grow: ' + await evalPromise);

  // 2. Re-entrant append of the same attribute during pause in
  // AppendAttributeInternal.
  pausedPromise = dp.Debugger.oncePaused();
  evalPromise = session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.setAttribute('new-attr-reentrant', 'outer-val');
    return el.getAttribute('new-attr-reentrant');
  })()`);
  await pausedPromise;
  await dp.Runtime.evaluate({
    expression: `(() => {
      const el = document.getElementById('target');
      el.setAttribute('new-attr-reentrant', 'inner-val');
    })()`
  });
  await dp.Debugger.resume();
  testRunner.log('new-attr-reentrant: ' + await evalPromise);

  // 3. Shrink attribute vector during pause in AppendAttributeInternal.
  pausedPromise = dp.Debugger.oncePaused();
  evalPromise = session.evaluate(`(() => {
    const el = document.getElementById('target');
    el.setAttribute('new-attr-shrink', 'val-shrink');
    return el.getAttribute('new-attr-shrink');
  })()`);
  await pausedPromise;
  await dp.Runtime.evaluate({
    expression: `(() => {
      const el = document.getElementById('target');
      for (let i = 0; i < 32; ++i) {
        el.removeAttribute('extra' + i);
      }
    })()`
  });
  await dp.Debugger.resume();
  testRunner.log('new-attr-shrink: ' + await evalPromise);

  testRunner.completeTest();
})
