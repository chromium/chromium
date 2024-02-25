(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startURL('../resources/dom-snapshot.html', 'Tests DOMSnapshot.getSnapshot method.');

  await session.evaluate(`
    var host = document.querySelector('#shadow-host').attachShadow({mode: 'open'});
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
    document.body.offsetWidth;
  `);

  function cleanupPaths(obj) {
    for (const key of Object.keys(obj)) {
      const value = obj[key];
      if (typeof value === 'string' && value.indexOf('/dom-snapshot/') !== -1)
        obj[key] = '<value>';
      else if (typeof value === 'string' && value.indexOf('file://') !== -1)
        obj[key] = '<string>' + value.replace(/.*(LayoutTests|web_tests)\//, '');
      else if (typeof value === 'object')
        cleanupPaths(value);
    }
    return obj;
  }

  const whitelist = ['transform', 'transform-origin', 'height', 'width', 'display', 'outline-color', 'color'];
  const response = await dp.DOMSnapshot.getSnapshot({'computedStyleWhitelist': whitelist, 'includeEventListeners': true});
  if (response.error)
    testRunner.log(response);
  else
    testRunner.log(cleanupPaths(response.result), null, ['documentURL', 'baseURL', 'frameId', 'backendNodeId', 'scriptId']);
  testRunner.completeTest();
})
