(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startHTML(
      `
       <style>
         button {
           position: absolute;
           width: 50px;
           height: 50px;
         }
         #btn-command { top: 100px; left: 100px; }
         #btn-popover1 { top: 200px; left: 100px; }
         #btn-popover2 { top: 300px; left: 100px; }

         [popover] {
           margin: 0;
           top: anchor(top);
           left: anchor(right);
         }
       </style>
       <button id="btn-command" commandfor="p1" command="show-popover"></button>
       <div popover id="p1"></div>

       <button id="btn-popover1" popovertarget="p2"></button>
       <button id="btn-popover2" popovertarget="p2"></button>
       <div popover id="p2"></div>
    `,
      'Tests that DOM.forceShowPopover determines the invoker correctly for implicit anchoring');
  await dp.Runtime.enable();
  await dp.DOM.enable();
  const doc = await dp.DOM.getDocument();

  async function getIds(selector) {
    const {result: {nodeId}} = await dp.DOM.querySelector({nodeId: doc.result.root.nodeId, selector});
    const {result: {node}} = await dp.DOM.describeNode({nodeId});
    return {nodeId, backendNodeId: node.backendNodeId};
  }

  const p1 = await getIds('#p1');
  const p2 = await getIds('#p2');
  const btnCommand = await getIds('#btn-command');
  const btnPopover1 = await getIds('#btn-popover1');
  const btnPopover2 = await getIds('#btn-popover2');

  async function forceShowPopover(nodeId, invokerNodeId) {
    const params = {nodeId, enable: true};
    if (invokerNodeId !== undefined) {
        params.invokerNodeId = invokerNodeId;
    }
    const {result, error} = await dp.DOM.forceShowPopover(params);
    if (error) {
        testRunner.log('Error: ' + error.message);
    }
  }

  async function checkPopoverPosition(id) {
    testRunner.log(
        (await dp.Runtime.evaluate({
          returnByValue: true,
          expression:
              `document.getElementById('${id}').getBoundingClientRect().top`
        })).result.result,
        `Popover ${id} top position: `);
  }

  testRunner.log('\\nTest 1: commandfor button acts as default invoker');
  await forceShowPopover(p1.nodeId);
  await checkPopoverPosition('p1');
  await dp.DOM.forceShowPopover({nodeId: p1.nodeId, enable: false});

  testRunner.log('\\nTest 2: first popovertarget button acts as default invoker');
  await forceShowPopover(p2.nodeId);
  await checkPopoverPosition('p2');
  await dp.DOM.forceShowPopover({nodeId: p2.nodeId, enable: false});

  testRunner.log('\\nTest 3: explicit invokerNodeId overrides default popovertarget button');
  await forceShowPopover(p2.nodeId, btnPopover2.backendNodeId);
  await checkPopoverPosition('p2');
  await dp.DOM.forceShowPopover({nodeId: p2.nodeId, enable: false});

  testRunner.log('\\nTest 4: explicit invalid invokerNodeId returns an error');
  await forceShowPopover(p2.nodeId, 999999);

  testRunner.completeTest();
})
