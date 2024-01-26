(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot-ua-shadow-tree.html', 'Tests DOMSnapshot.getSnapshot method with includeUserAgentShadowTree defaulted to false.');

  await session.evaluate(`
    var shadowroot = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var textarea = document.createElement('textarea');
    textarea.value = 'hello hello!';
    var video = document.createElement('video');
    video.src = 'test.webm';
    shadowroot.appendChild(textarea);
    shadowroot.appendChild(video);
  `);

  function cleanupPaths(obj) {
    for (const key of Object.keys(obj)) {
      const value = obj[key];
      if (typeof value === 'string' && value.indexOf('/dom-snapshot/') !== -1)
        obj[key] = '<value>';
      else if (typeof value === 'object')
        cleanupPaths(value);
    }
    return obj;
  }

  const response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': [], 'includeEventListeners': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(cleanupPaths(response.result), null, ['documentURL', 'baseURL', 'frameId', 'backendNodeId', 'layoutTreeNodes']);
  testRunner.completeTest();
})
