(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startHTML(
      `
      <style>
        ::view-transition-group(*),
        ::view-transition-old(*),
        ::view-transition-new(*) {
          animation-duration: 600s;
        }
      </style>
      <div style='width:100px; height:100px; view-transition-name: target;'></div>
      <div style='width:100px; height:100px; view-transition-name: other;'></div>
      <div id=removeme></div>`,
      'Tests that DOM breakpoints do not pause for DOM mutations during style recalc, where running script (e.g. skipping a view transition) is unsafe.');

  await session.evaluateAsync(`
     new Promise(async (resolve) => {
       window.vt = document.startViewTransition();
       await vt.ready;
       getComputedStyle(document.documentElement,
                        '::view-transition-new(root)').background;
       resolve();
     });
  `);

  await dp.DOM.enable();
  await dp.Debugger.enable();
  const {result} = await dp.DOM.getDocument({depth: -1});
  const viewTransition = result.root.children[0].pseudoElements[0];
  const group =
      viewTransition.pseudoElements.find(p => p.pseudoIdentifier === 'target');
  const imagePair = group.pseudoElements[0];
  testRunner.log('Breakpoint node: ' + imagePair.nodeName + '(' +
                 imagePair.pseudoIdentifier + ')');
  await dp.DOMDebugger.setDOMBreakpoint(
      {nodeId: imagePair.nodeId, type: 'node-removed'});

  let phase = 'style-recalc';
  dp.Debugger.onPaused(async message => {
    testRunner.log('Paused on ' + message.params.data.type + ' during ' +
                   phase);
    if (phase === 'style-recalc') {
      // Ending the transition while paused inside style recalc would clear
      // the list of view transition names that style recalc is iterating.
      await session.evaluate('vt.skipTransition()');
    }
    dp.Debugger.resume();
  });

  // Hiding the group removes ::view-transition-image-pair(target) during
  // style recalc.
  await session.evaluate(`
    const style = document.createElement('style');
    style.textContent = '::view-transition-group(target) { display: none; }';
    document.head.appendChild(style);
    getComputedStyle(document.documentElement).color;
  `);
  testRunner.log('Style recalc completed');

  // Script-initiated removals should still pause.
  phase = 'script-removal';
  const removeMe = (await dp.DOM.querySelector({
                     nodeId: result.root.nodeId,
                     selector: '#removeme'
                   })).result;
  await dp.DOMDebugger.setDOMBreakpoint(
      {nodeId: removeMe.nodeId, type: 'node-removed'});
  await session.evaluate(`document.getElementById('removeme').remove()`);
  testRunner.log('Script removal completed');

  testRunner.completeTest();
})
