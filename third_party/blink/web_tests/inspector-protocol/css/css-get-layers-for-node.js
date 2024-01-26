(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
<style>
 @layer base, ext, base.nested, empty;
 @import url(resources/stylesheet.css) layer(stylesheet);
 @layer base {
  body {
    color: green;
  }

  @layer nested {
   body {
    color: orange;
   }
  }
 }
 @layer ext {
  body {
   color: red;
  }
 }
</style>
<script>
 const div = document.createElement('div');
 const shadowRoot = div.attachShadow({mode: 'open'});
 const style = document.createElement('style');

 style.textContent = '@layer shadow1, shadow2';
 shadowRoot.appendChild(style);
 document.documentElement.appendChild(div);

 const span = document.createElement('span');
 span.textContent = 'shadow root with no styles';
 const shadowRoot2 = span.attachShadow({mode: 'open'});
 const h1 = document.createElement('h1');
 shadowRoot2.appendChild(h1);
 document.documentElement.appendChild(span);
</script>
`, 'Verify that layers are reported properly.');

  var CSSHelper = await testRunner.loadScript('../resources/css-helper.js');
  var cssHelper = new CSSHelper(testRunner, dp);
  await dp.DOM.enable();
  await dp.CSS.enable();
  const documentNodeId = await cssHelper.requestDocumentNodeId();

  testRunner.log('Layers for document scope: ');
  const bodyNodeId = await cssHelper.requestNodeId(documentNodeId, 'body');
  const response1 = await dp.CSS.getLayersForNode({nodeId: bodyNodeId});
  const documentLayers = response1.result.rootLayer;
  testRunner.log(documentLayers)

  testRunner.log('Layers for shadow root scope: ');
  const shadowHostId = await cssHelper.requestNodeId(documentNodeId, 'div');
  const shadowHostResp = await dp.DOM.describeNode({nodeId: shadowHostId, pierce: true, depth: -1});
  const styleBackendId = shadowHostResp.result.node.shadowRoots[0].children[0].backendNodeId;
  const styleIdResp = await dp.DOM.pushNodesByBackendIdsToFrontend({backendNodeIds: [styleBackendId]});
  const styleId = styleIdResp.result.nodeIds[0];
  const response2 = await dp.CSS.getLayersForNode({nodeId: styleId});
  const shadowLayers = response2.result.rootLayer;
  testRunner.log(shadowLayers)

  testRunner.log('Layers for empty shadow root scope: ');
  const shadowHostId2 = await cssHelper.requestNodeId(documentNodeId, 'span');
  const shadowHostResp2 = await dp.DOM.describeNode({nodeId: shadowHostId2, pierce: true, depth: -1});
  const h1BackendId = shadowHostResp2.result.node.shadowRoots[0].children[0].backendNodeId;
  const h1IdResp = await dp.DOM.pushNodesByBackendIdsToFrontend({backendNodeIds: [h1BackendId]});
  const h1Id = h1IdResp.result.nodeIds[0];
  const response3 = await dp.CSS.getLayersForNode({nodeId: h1Id});
  const shadowLayers2 = response3.result.rootLayer;
  testRunner.log(shadowLayers2)

  testRunner.completeTest();
});
