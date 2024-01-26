(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
    const {session, dp} = await testRunner.startHTML(`
  <style>
    #wrapper {
      display: contents;
    }
  </style>
  <div id="wrapper">
    <div id="child">
    </div>
  </div>`, 'The test verifies CSS.getMatchedStylesForNode returns the correct parentNodeId.');

    const CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
    const cssHelper = new CSSHelper(testRunner, dp);

    const getParentLayoutNodeId = async (nodeId) => {
      const {result} = await dp.CSS.getMatchedStylesForNode({'nodeId': nodeId});

      return result.parentLayoutNodeId;
    }

    await dp.DOM.enable();
    await dp.CSS.enable();

    const documentNodeId = await cssHelper.requestDocumentNodeId();

    const wrapper = await cssHelper.requestNodeId(documentNodeId, '#wrapper');
    const child = await cssHelper.requestNodeId(documentNodeId, '#child');

    const wrapperParentNodeId = await getParentLayoutNodeId(wrapper);
    let childParentNodeId = await getParentLayoutNodeId(child);
    testRunner.log("Parent node id of the #wrapper: " + wrapperParentNodeId);
    testRunner.log("Parent node id of the #child when #wrapper has 'display: contents': " + childParentNodeId);

    await session.evaluate(
      () =>
        document.querySelector('#wrapper').style.display = 'block');

    childParentNodeId = await getParentLayoutNodeId(child);
    testRunner.log("Parent node id of the #child after #wrapper display was changed to 'block': " + childParentNodeId);

    testRunner.completeTest();
  });
