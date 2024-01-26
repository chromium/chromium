(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startHTML(`
  <script src="../../resources/testdriver.js"></script>
  <script src="../../resources/testdriver-vendor.js"></script>
  <script src="../../fast/forms/resources/picker-common.js"></script>
  <select>
    <option>OPTION1</option>
    <option>OPTION2</option>
    <option>OPTION3</option>
    <option>OPTION4</option>
  </select>
  `, '[crbug/1311561] Tests that auto dark mode emulation from DevTools correctly emulates dark mode for select element');

  async function logScreenshotData() {
    const response = await dp.Page.captureScreenshot();
    const imageData = response.result.data;
    testRunner.log(`data:image/png;base64,${imageData}`);
  }

  await dp.Emulation.enable();
  await dp.Page.enable();

  testRunner.log("=== Before auto dark mode (autoDarkMode and prefers-color-scheme override) is not enabled ===");
  await session.evaluateAsync(`openPicker(document.querySelector("select"))`);
  await logScreenshotData();

  await dp.Emulation.setAutoDarkModeOverride({enabled: true});
  await dp.Emulation.setEmulatedMedia({
    type: '',
    features: [{
      name: 'prefers-color-scheme',
      value: 'dark'
    }]
  });

  testRunner.log("\n=== After auto dark mode (autoDarkMode and prefers-color-scheme override) is enabled ===");
  await session.evaluateAsync(`openPicker(document.querySelector("select"))`);
  await logScreenshotData();

  testRunner.completeTest();
});
