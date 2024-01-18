(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests that Page.setInterceptFileChooserDialog works as expected');

  await dp.Page.enable();
  await dp.Runtime.enable();
  dp.Runtime.onConsoleAPICalled(event => {
    testRunner.log(event.params.args[0].value);
  });
  await session.evaluate(() => {
    window.getSelectedFiles = (picker) => JSON.stringify(Array.from(picker.files).map(file => file.name));
    window.LOG = (...args) => console.log(args.join(' '));
  });

  await dp.Page.setInterceptFileChooserDialog({enabled: true});

  function setFileInputFunction(fileNames) {
    const files = fileNames.map(fileName => new File(['test'], fileName));
    const dt = new DataTransfer();
    for (const file of files)
      dt.items.add(file);
    this.files = dt.files;
    this.dispatchEvent(new Event('input', { 'bubbles': true }));
  }

  async function setInputFiles(backendNodeId, fileNames) {
    const response = await dp.DOM.resolveNode({ backendNodeId });
    const object = response.result.object;
    await dp.Runtime.callFunctionOn({
      objectId: object.objectId,
      functionDeclaration: setFileInputFunction.toString(),
      arguments: [ { value: fileNames } ]
    });
  }

  testRunner.runTestSuite([
    async function testAcceptFile() {
      dp.Page.onceFileChooserOpened(event => {
        testRunner.log('file chooser mode: ' + event.params.mode);
        setInputFiles(event.params.backendNodeId, ['path1']);
        return true;
      });
      await session.evaluateAsyncWithUserGesture(async () => {
        const picker = document.createElement('input');
        picker.type = 'file';
        picker.click();
        await new Promise(x => picker.oninput = x);
        LOG('selected files: ' + getSelectedFiles(picker));
      });
    },

    async function testAcceptMultipleFiles() {
      dp.Page.onceFileChooserOpened(event => {
        testRunner.log('file chooser mode: ' + event.params.mode);
        setInputFiles(event.params.backendNodeId, ['path1', 'path2']);
        return true;
      });
      await session.evaluateAsyncWithUserGesture(async () => {
        const picker = document.createElement('input');
        picker.type = 'file';
        picker.setAttribute('multiple', true);
        picker.click();
        await new Promise(x => picker.oninput = x);
        LOG('selected files: ' + getSelectedFiles(picker));
      });
    },

    async function testShowPickerAPI() {
      dp.Page.onceFileChooserOpened(event => {
        testRunner.log('file chooser mode: ' + event.params.mode);
        setInputFiles(event.params.backendNodeId, ['path1']);
        return true;
      });
      await session.evaluateAsyncWithUserGesture(async () => {
        const picker = document.createElement('input');
        picker.type = 'file';
        picker.showPicker();
        await new Promise(x => picker.oninput = x);
        LOG('selected files: ' + getSelectedFiles(picker));
      });
    },

    async function testOpenFilePickerAPI() {
      const [event] = await Promise.all([
        dp.Page.onceFileChooserOpened(),
        session.evaluateAsyncWithUserGesture(async () => {
          try {
            await window.showOpenFilePicker();
          }
          catch (e) {
            LOG(e.message);
          }
        }),
      ]);
    },

    async function testSaveFilePickerAPI() {
      const [event] = await Promise.all([
        dp.Page.onceFileChooserOpened(),
        session.evaluateAsyncWithUserGesture(async () => {
          try {
            await window.showSaveFilePicker();
          }
          catch (e) {
            LOG(e.message);
          }
        }),
      ]);
    },

    async function testDirectoryPickerAPI() {
      const [event] = await Promise.all([
        dp.Page.onceFileChooserOpened(),
        session.evaluateAsyncWithUserGesture(async () => {
          try {
            await window.showDirectoryPicker();
          }
          catch (e) {
            LOG(e.message);
          }
        }),
      ]);
    },

    async function testErrors() {
      testRunner.log('Try enabling file interception in multiclient');
      const session2 = await page.createSession();
      await session2.protocol.Page.enable();
      testRunner.log(await session2.protocol.Page.setInterceptFileChooserDialog({enabled: true}));

      // Trigger file chooser.
      const [event] = await Promise.all([
        dp.Page.onceFileChooserOpened(),
        session.evaluateAsyncWithUserGesture(() => {
          const picker = document.createElement('input');
          picker.type = 'file';
          picker.click();
        }),
      ]);
    },
  ]);
})
