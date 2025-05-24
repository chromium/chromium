(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests that Page.enable(enableFileChooserOpenedEvent) enables `Page.fileChooserOpened` events');

  await dp.Page.enable({enableFileChooserOpenedEvent: true});
  dp.Page.onFileChooserOpened(event => {
    testRunner.log(event, 'Page.fileChooserOpened');
  });

  testRunner.runTestSuite([
    async function testClickFileInput() {
      void session.evaluateAsyncWithUserGesture(async () => {
        const picker = document.createElement('input');
        picker.type = 'file';
        picker.click();
      });

      await dp.Page.onceFileChooserOpened();
    },

    async function testShowPickerAPI() {
      void session.evaluateAsyncWithUserGesture(async () => {
        const picker = document.createElement('input');
        picker.type = 'file';
        picker.showPicker();
      });

      await dp.Page.onceFileChooserOpened();
    },

    async function testOpenFilePickerAPI() {
      void session.evaluateAsyncWithUserGesture(async () => {
        await window.showOpenFilePicker();
      });

      await dp.Page.onceFileChooserOpened();
    },

    async function testSaveFilePickerAPI() {
      void session.evaluateAsyncWithUserGesture(async () => {
        await window.showSaveFilePicker();
      });

      await dp.Page.onceFileChooserOpened();
    },

    async function testDirectoryPickerAPI() {
      void session.evaluateAsyncWithUserGesture(async () => {
        await window.showDirectoryPicker();
      })
      await dp.Page.onceFileChooserOpened();
    }
  ]);
})
