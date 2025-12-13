(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session, page} = await testRunner.startHTML(
      `<div popover="manual" id="popover"></div>
       <div popover=auto id=p1>
         <div popover=hint id=p2></div>
       </div>
       <div popover=hint id=p3></div>
    `,
      'Tests the DOM.forceShowPopover method');
  await dp.Runtime.enable();
  await dp.DOM.enable();
  const doc = await dp.DOM.getDocument();

  const elementIds = ['#popover', '#p1', '#p2', '#p3'];
  const nodeIds = await Promise.all(elementIds.map(
      async (selector) => (await dp.DOM.querySelector(
                               {nodeId: doc.result.root.nodeId, selector}))
                              .result.nodeId));
  const nodeIdMap = new Map(nodeIds.map((v, i) => [v, elementIds[i]]));

  function nodeName(nodeId) {
    return nodeIdMap.get(nodeId) ?? nodeId;
  }

  async function forceShowPopover(nodeId) {
    const {result} = await dp.DOM.forceShowPopover({nodeId, enable: true});
    testRunner.log(
        result.nodeIds.map(nodeName).toSorted(),
        `closed popovers when force-showing node ${nodeName(nodeId)}`);
  }

  async function forceHidePopover(nodeId) {
    const {result} = await dp.DOM.forceShowPopover({nodeId, enable: false});
    testRunner.log(
        result.nodeIds.map(nodeName).toSorted(),
        `closed popovers when force-hiding node ${nodeName(nodeId)}`);
  }

  async function checkOpenPopovers() {
    testRunner.log(
        (await dp.Runtime.evaluate({
          returnByValue: true,
          expression:
              `Array.from(document.querySelectorAll(':popover-open')).map(e => e.getAttribute('id'))`
        })).result.result,
        'Currently open popovers: ');
  }

  async function htmlElementShowPopover(id = 'popover') {
    testRunner.log(
        (await dp.Runtime.evaluate({
          expression: `document.getElementById('${
              id}').showPopover(); JSON.stringify(Array.from(document.querySelectorAll(':popover-open')).map(e => e.id).sort())`
        })).result.result,
        `Open popovers after #${id}.showPopover(): `);
  }
  async function htmlElementHidePopover(id = 'popover') {
    testRunner.log(
        (await dp.Runtime.evaluate({
          expression: `document.getElementById('${
              id}').hidePopover(); JSON.stringify(Array.from(document.querySelectorAll(':popover-open')).map(e => e.id).sort())`
        })).result.result,
        `Open popovers after #${id}.hidePopover(): `);
  }

  testRunner.log('Compare force/unforce orders with hint and auto stacks');
  await forceShowPopover(nodeIds[1]);
  await forceShowPopover(nodeIds[3]);
  await forceHidePopover(nodeIds[1]);
  await forceHidePopover(nodeIds[3]);
  await checkOpenPopovers();

  await forceShowPopover(nodeIds[3]);
  await htmlElementShowPopover('p1');
  await htmlElementHidePopover('p1');
  await checkOpenPopovers();
  await forceHidePopover(nodeIds[3]);
  await htmlElementHidePopover('p1');
  await checkOpenPopovers();


  testRunner.log(
      'First, check interleaving forceShowPopover calls with showPopover()/hidePopover()');
  await checkOpenPopovers();

  await htmlElementShowPopover();
  await htmlElementHidePopover();

  testRunner.log('force-open:');
  await forceShowPopover(nodeIds[0]);
  await checkOpenPopovers();
  await htmlElementHidePopover();

  testRunner.log('force-close:');
  await forceHidePopover(nodeIds[0]);
  await checkOpenPopovers();
  await htmlElementShowPopover();
  await checkOpenPopovers();


  testRunner.log('force-open:');
  await forceShowPopover(nodeIds[0]);
  await checkOpenPopovers();

  await htmlElementHidePopover();
  await checkOpenPopovers();

  await forceHidePopover(nodeIds[0]);

  testRunner.log(
      '\nSecond, test forceShowPopover calls for popover stack in different orders');
  testRunner.log('force-opened in order:');
  await forceShowPopover(nodeIds[1]);
  await forceShowPopover(nodeIds[2]);
  await forceShowPopover(nodeIds[3]);
  await checkOpenPopovers();

  testRunner.log('force-closed in order:');
  await forceHidePopover(nodeIds[3]);
  await forceHidePopover(nodeIds[2]);
  await forceHidePopover(nodeIds[1]);
  await checkOpenPopovers();

  testRunner.log('force-opened in order:');
  await forceShowPopover(nodeIds[1]);
  await forceShowPopover(nodeIds[2]);
  await forceShowPopover(nodeIds[3]);
  await checkOpenPopovers();

  testRunner.log('force-closed out of order:');
  await forceHidePopover(nodeIds[2]);
  await forceHidePopover(nodeIds[3]);
  await forceHidePopover(nodeIds[1]);
  await checkOpenPopovers();

  testRunner.log('force-opened out of order:');
  await forceShowPopover(nodeIds[3]);
  await forceShowPopover(nodeIds[2]);
  await forceShowPopover(nodeIds[1]);
  await checkOpenPopovers();

  testRunner.log('force-closed out of order:');
  await forceHidePopover(nodeIds[2]);
  await forceHidePopover(nodeIds[3]);
  await forceHidePopover(nodeIds[1]);
  await checkOpenPopovers();

  testRunner.log('hidePopover() out of order:');
  await htmlElementShowPopover('p1');
  await forceShowPopover(nodeIds[2]);
  await htmlElementHidePopover('p1');

  testRunner.completeTest();
})
