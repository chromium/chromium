(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startURL('resources/dom-get-element-by-relation.html', 'Tests DOM.getElementByRelation command.');

  await dp.DOM.enable();
  const getDocumentResponse = await dp.DOM.getDocument();

  const popoverOpener1 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.popover-opener-1' })).result.nodeId;
  const target1ById = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#my-popover-1' })).result.nodeId;
  const popoverTarget1 = await dp.DOM.getElementByRelation({nodeId: popoverOpener1, relation: 'PopoverTarget'});
  const interestTarget1 = await dp.DOM.getElementByRelation({nodeId: popoverOpener1, relation: 'InterestTarget'});
  testRunner.log('Node Id from query selector and getElementByRelation should be the same:');
  testRunner.log(target1ById === popoverTarget1.result.nodeId);
  testRunner.log('Node Id of PopoverTarget and InterestTarget should be the same because they point to the same element:');
  testRunner.log(popoverTarget1.result.nodeId === interestTarget1.result.nodeId);

  const popoverOpener2 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.popover-opener-2' })).result.nodeId;
  const emptyPopoverTarget = await dp.DOM.getElementByRelation({nodeId: popoverOpener2, relation: 'PopoverTarget'});
  const emptyInterestTarget = await dp.DOM.getElementByRelation({nodeId: popoverOpener2, relation: 'InterestTarget'});
  testRunner.log('non-existent target id should be zero: ');
  testRunner.log(emptyPopoverTarget.result.nodeId);
  testRunner.log(emptyInterestTarget.result.nodeId);

  const popoverOpener3 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.popover-opener-3' })).result.nodeId;
  const popoverTargetByNonFormControlEl = await dp.DOM.getElementByRelation({nodeId: popoverOpener3, relation: 'PopoverTarget'});
  const interestTargetByInvalidEl = await dp.DOM.getElementByRelation({nodeId: popoverOpener3, relation: 'InterestTarget'});
  testRunner.log('non-existent target id should be zero: ');
  testRunner.log(popoverTargetByNonFormControlEl.result.nodeId);
  testRunner.log(interestTargetByInvalidEl.result.nodeId);

  // Verify that it works with popover/interest target set via JavaScript API.
  await dp.Runtime.evaluate({ expression: `
    const popoverOpener2 = document.querySelector('.popover-opener-2');
    const myPopover2 = document.getElementById('my-popover-2');
    popoverOpener2.popoverTargetElement = myPopover2;
    popoverOpener2.interestForElement = myPopover2;
  `});
  const popoverTarget2 = await dp.DOM.getElementByRelation({nodeId: popoverOpener2, relation: 'PopoverTarget'});
  const interestTarget2 = await dp.DOM.getElementByRelation({nodeId: popoverOpener2, relation: 'InterestTarget'});
  const target2ById = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#my-popover-2' })).result.nodeId;
  testRunner.log('Node Id from query selector and getElementByRelation should be the same:');
  testRunner.log(target2ById === popoverTarget2.result.nodeId);
  testRunner.log('Node Id of PopoverTarget and InterestTarget should be the same because they point to the same element:');
  testRunner.log(popoverTarget2.result.nodeId === interestTarget2.result.nodeId);

  // Verify that interestfor works regardless of popovertarget.
  const popoverOpener4 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.popover-opener-4' })).result.nodeId;
  const target3ById = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#my-popover-3' })).result.nodeId;
  const interestTarget3 = await dp.DOM.getElementByRelation({nodeId: popoverOpener4, relation: 'InterestTarget'});
  testRunner.log('Node Id from query selector and getElementByRelation should be the same:');
  testRunner.log(target3ById === interestTarget3.result.nodeId);

  // Verify that commandfor relationship can be corrected retrieved.
  const dialogOpener1 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.dialog-opener-1' })).result.nodeId;
  const dialogOpener2 = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '.dialog-opener-2' })).result.nodeId;
  const targetDialogById = (await dp.DOM.querySelector({nodeId: getDocumentResponse.result.root.nodeId, selector: '#my-dialog' })).result.nodeId;
  const commandFor1 = await dp.DOM.getElementByRelation({nodeId: dialogOpener1, relation: 'CommandFor'});
  const emptyCommandFor = await dp.DOM.getElementByRelation({nodeId: dialogOpener2, relation: 'CommandFor'});
  testRunner.log('Node Id from query selector and getElementByRelation should be the same:');
  testRunner.log(targetDialogById === commandFor1.result.nodeId);
  testRunner.log('non-existent target id should be zero: ');
  testRunner.log(emptyCommandFor.result.nodeId);

  await dp.Runtime.evaluate({ expression: `
    const dialogOpener2 = document.querySelector('.dialog-opener-2');
    const myDialog = document.getElementById('my-dialog');
    dialogOpener2.commandForElement = myDialog;
    dialogOpener2.command = 'show-modal';
  `});
  const commandFor2 = await dp.DOM.getElementByRelation({nodeId: dialogOpener2, relation: 'CommandFor'});
  testRunner.log('Node Id from query selector and getElementByRelation should be the same:');
  testRunner.log(targetDialogById === commandFor2.result.nodeId);

  testRunner.completeTest();
})
