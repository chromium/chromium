(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
    <input type="file" id="file" name="avatar" accept="image/png, image/jpeg">`, 'Tests DOM.getFileInfo method.');

  let response = await dp.Runtime.evaluate({ expression: `document.querySelector('#file')` });
  const path = window.location.href.replace(/.*test=/, '');
  await dp.DOM.setFileInputFiles({objectId: response.result.result.objectId, files: [path]});

  response = await dp.Runtime.evaluate({ expression: `document.querySelector('#file').files[0]` });
  const info = await dp.DOM.getFileInfo({objectId: response.result.result.objectId});
  const path2 = info.result.path;
  testRunner.log('Paths match: ' + (path === path2));
  testRunner.completeTest();
})
