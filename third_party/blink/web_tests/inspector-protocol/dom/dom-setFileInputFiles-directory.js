(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startHTML(`
    <input type="file" id="file" name="avatar" accept="image/png, image/jpeg">`, 'Tests DOM.setFileInputFiles method with a directory.');

  let response = await dp.Runtime.evaluate({ expression: `document.querySelector('#file')` });
  const test_path = window.location.href.replace(/.*test=/, '');
  const test_directory = test_path.substring(0, test_path.lastIndexOf('/') + 1);
  testRunner.log(`Test directory suffix: ${test_directory.split('/').slice(-3).join('/')}`);
  await dp.DOM.setFileInputFiles({objectId: response.result.result.objectId, files: [test_directory]});

  response = await dp.Runtime.evaluate({ expression: `document.querySelector('#file').files.length` });
  if(response.result.result.value !== 1) {
    testRunner.log(`FAIL: only a single file is expected in the file input, but got ${response.result.result.value}.`);
    testRunner.completeTest();
    return;
  }

  response = await dp.Runtime.evaluate({ expression: `document.querySelector('#file').files[0]` });
  const info = await dp.DOM.getFileInfo({objectId: response.result.result.objectId});
  const file_info_path = info.result.path;
  testRunner.log(`File info suffix: ${file_info_path.split('/').slice(-3).join('/')}`);

  if(test_directory !== file_info_path) {
    testRunner.log(`FAIL: file info should be equal the directory`);
  }
  testRunner.completeTest();
})
