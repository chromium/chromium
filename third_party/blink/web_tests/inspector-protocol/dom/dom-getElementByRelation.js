(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('resources/dom-get-element-by-relation.html', 'Tests DOM.getElementByRelation command.');

  await dp.DOM.enable();
  const getDocumentResponse = await dp.DOM.getDocument();
  const popoverOpener1 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.popover-opener-1' })).result.nodeId;
  const popoverTarget1ById = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#my-popover-1' })).result.nodeId;
  const popoverTarget1 = await dp.DOM.getElementByRelation({nodeId: popoverOpener1, relation: 'PopoverTarget'});
  testRunner.log('Node Id from query selector and getElementByRelation should be the same:');
  testRunner.log(popoverTarget1ById === popoverTarget1.result.nodeId);

  const popoverOpener2 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.popover-opener-2' })).result.nodeId;
  const emptyTarget = await dp.DOM.getElementByRelation({nodeId: popoverOpener2, relation: 'PopoverTarget'});
  testRunner.log('non-existent target id should be zero: ');
  testRunner.log(emptyTarget.result.nodeId);

  const popoverOpener3 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.popover-opener-3' })).result.nodeId;
  const targetByNonFormControlEl = await dp.DOM.getElementByRelation({nodeId: popoverOpener3, relation: 'PopoverTarget'});
  testRunner.log('non-existent target id should be zero: ');
  testRunner.log(targetByNonFormControlEl.result.nodeId);

  // Verify that it works with popover target set via JavaScript API.
  await dp.Runtime.evaluate({ expression: `
    const popoverOpener2 = document.querySelector('.popover-opener-2');
    const myPopover2 = document.getElementById('my-popover-2');
    popoverOpener2.popoverTargetElement = myPopover2;
  `});
  const popoverTarget2 = await dp.DOM.getElementByRelation({nodeId: popoverOpener2, relation: 'PopoverTarget'});
  const popoverTarget2ById = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#my-popover-2' })).result.nodeId;
  testRunner.log('Node Id from query selector and getElementByRelation should be the same:');
  testRunner.log(popoverTarget2ById === popoverTarget2.result.nodeId);

  testRunner.completeTest();
})
