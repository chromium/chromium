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

  await dp.DOMDebugger.removeDOMBreakpoint(
      {nodeId, type: 'attribute-modified'});
  await session.evaluate(`(() => {
    document.body.innerHTML =
        '<div contenteditable id="host">' +
        '<p id="orig" style="color:red" a1="1" a2="2" a3="3" a4="4" a5="5">' +
        'text</p></div>';
    window.orig = document.getElementById('orig');
    window.orig.setAttribute('a6', '6');
    const range = document.createRange();
    range.selectNodeContents(window.orig);
    getSelection().removeAllRanges();
    getSelection().addRange(range);
  })()`);
  const {result: {root}} = await dp.DOM.getDocument();
  const {result: {nodeId: hostId}} =
      await dp.DOM.querySelector({nodeId: root.nodeId, selector: '#host'});
  await dp.DOMDebugger.setDOMBreakpoint(
      {nodeId: hostId, type: 'subtree-modified'});

  pausedPromise = dp.Debugger.oncePaused();
  evalPromise = session.evaluate(`(() => {
    document.execCommand('formatBlock', false, 'div');
    return document.getElementById('host').innerHTML;
  })()`);
  await pausedPromise;
  pausedPromise = dp.Debugger.oncePaused();
  await dp.Debugger.resume();
  const {params: {data}} = await pausedPromise;
  const {result: {node: block}} =
      await dp.DOM.describeNode({nodeId: data.targetNodeId});
  testRunner.log('New block before style copy: ' + block.nodeName);
  testRunner.log('New block attributes: ' + JSON.stringify(block.attributes));
  await dp.DOMDebugger.setDOMBreakpoint(
      {nodeId: block.nodeId, type: 'attribute-modified'});
  await dp.DOMDebugger.removeDOMBreakpoint(
      {nodeId: hostId, type: 'subtree-modified'});

  pausedPromise = dp.Debugger.oncePaused();
  await dp.Debugger.resume();
  const {params: {data: attributeData}} = await pausedPromise;
  testRunner.log('Paused before style copy: ' + attributeData.type);
  testRunner.log('Paused on new block: ' +
                 (attributeData.nodeId === block.nodeId));
  testRunner.log('Source attribute storage mutated: ' +
                 await session.evaluate(`(() => {
    const before = window.orig.attributes.length;
    for (let i = 0; i < 64; ++i) {
      window.orig.setAttribute('extra' + i, 'v');
    }
    return before + ' -> ' + window.orig.attributes.length;
  })()`));
  await dp.DOMDebugger.removeDOMBreakpoint(
      {nodeId: block.nodeId, type: 'attribute-modified'});
  await dp.Debugger.resume();
  testRunner.log('Block after style copy: ' + await evalPromise);

  testRunner.completeTest();
})
