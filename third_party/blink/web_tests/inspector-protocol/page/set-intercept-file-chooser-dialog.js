(async function(testRunner) {
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

  // Note: this test must be run from the file:// scheme.
  const path1 = window.location.href.replace(/.*test=/, '');
  const path2 = path1.substring(0, path1.lastIndexOf('.')) + '-expected.txt';

  testRunner.runTestSuite([
    async function testAcceptFile() {
      dp.Page.onceFileChooserOpened(event => {
        testRunner.log('file chooser mode: ' + event.params.mode);
        dp.Page.handleFileChooser({
          action: 'accept',
          files: [path1],
        });
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
        dp.Page.handleFileChooser({
          action: 'accept',
          files: [path1, path2],
        });
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

    async function testResetInput() {
      // Handle event twice: first to select files, then to reset them.
      let counter = 0;
      dp.Page.onceFileChooserOpened(event => {
        ++counter;
        dp.Page.handleFileChooser({
          action: 'accept',
          files: counter === 1 ? [path1] : [],
        });
        return counter === 2;
      });
      await session.evaluateAsyncWithUserGesture(async () => {
        const picker = document.createElement('input');
        picker.type = 'file';
        // 1. Summon file chooser and check files
        picker.click();
        await new Promise(x => picker.oninput = x);
        LOG('first selected files: ' + getSelectedFiles(picker));
        // 2. Wait a new task: file chooser might be requested only once from a task.
        await new Promise(x => setTimeout(x, 0));
        // 3. Summon file chooser one more time.
        picker.click();
        await new Promise(x => picker.oninput = x);
        LOG('second selected files: ' + getSelectedFiles(picker));
      });
    },

    async function testErrors() {
      testRunner.log('Try handling non-existing file chooser.');
      testRunner.log(await dp.Page.handleFileChooser({
          action: 'accept',
          files: [path1, path2],
      }));

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

      testRunner.log('Test file chooser fails when accepting multiple files for a non-multiple file chooser');
      testRunner.log(await dp.Page.handleFileChooser({
          action: 'accept',
          files: [path1, path2],
      }));

      testRunner.log('Test wrong action');
      testRunner.log(await dp.Page.handleFileChooser({
          action: 'badaction',
          files: [path1],
      }));

      testRunner.log('Test trying to handle already-handled file chooser');
      // Try to handle file chooser twice.
      await dp.Page.handleFileChooser({action: 'cancel'});
      testRunner.log(await dp.Page.handleFileChooser({
        action: 'accept',
        files: []
      }));
    },
  ]);
})
