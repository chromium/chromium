(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <input type="file" id="file" name="avatar" accept="image/png, image/jpeg">`, 'Tests DOM.setFileInputFiles method with a non existing file.');

  let response = await dp.Runtime.evaluate({ expression: `document.querySelector('#file')` });
  const test_path = "/SOME_NONE_EXISTING_FILE";
  await dp.DOM.setFileInputFiles({objectId: response.result.result.objectId, files: [test_path]});

  response = await dp.Runtime.evaluate({ expression: `document.querySelector('#file').files[0]` });
  const info = await dp.DOM.getFileInfo({objectId: response.result.result.objectId});
  const file_info_path = info.result.path;
  testRunner.log(`File info path: ${file_info_path}`);
  testRunner.completeTest();
})
