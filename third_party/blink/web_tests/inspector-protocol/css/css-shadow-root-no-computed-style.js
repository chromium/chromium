(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(
    `<div id='shadow-host'></div>`,
    'Tests that the shadow root itself has no computed style and there is no DCHECK failure');

  await session.evaluate(() => {
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    host.appendChild(document.createElement("div"));
  });

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);

  await dp.DOM.enable();
  await dp.CSS.enable();
  var documentNodeId = await cssHelper.requestDocumentNodeId();
  var shadowHostId = await cssHelper.requestNodeId(documentNodeId, '#shadow-host');
  const shadowHostResp = await dp.DOM.describeNode({nodeId: shadowHostId, pierce: true, depth: -1});
  const shadowRootBackendId = shadowHostResp.result.node.shadowRoots[0].backendNodeId;
  const shadowRootIdResp = await dp.DOM.pushNodesByBackendIdsToFrontend({backendNodeIds: [shadowRootBackendId]});
  const shadowRootId = shadowRootIdResp.result.nodeIds[0];
  var matchedStyles = await dp.CSS.getComputedStyleForNode({'nodeId': shadowRootId});
  testRunner.log(matchedStyles.error.message);
  testRunner.completeTest();
})
