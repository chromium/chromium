(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(`Tests oopif target info properties.`);

  await dp.Page.enable();
  const attachedPromise = dp.Target.onceAttachedToTarget();
  const frameDetachedPromise = dp.Page.onceFrameDetached();
  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false});

  // This page looks like "main -> iframe -> oopif".
  dp.Page.navigate({url: testRunner.url('../resources/oopif_in_iframe.html')});
  const targetInfo = (await attachedPromise).params.targetInfo;
  await frameDetachedPromise;  // Wait for the local->remote swap.
  const frameTree = (await dp.Page.getFrameTree()).result.frameTree;

  testRunner.log(`one child frame in the tree: ${frameTree.childFrames.length === 1 && !frameTree.childFrames[0].childFrames?.length}`);
  testRunner.log(`oopif parentFrameId matches: ${targetInfo.parentFrameId === frameTree.childFrames[0].frame.id}`);
  testRunner.completeTest();
})
