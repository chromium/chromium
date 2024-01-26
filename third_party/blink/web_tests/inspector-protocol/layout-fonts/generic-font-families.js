(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const genericFamilySettingsMap = {
    'initial': 'Standard',
    'serif': 'Serif',
    'sans-serif': 'SansSerif',
    'cursive': 'Cursive',
    'fantasy': 'Fantasy',
    'monospace': 'Fixed',
    'math': 'Math',
  };

  for (const family_name in genericFamilySettingsMap) {
    let setting_name = genericFamilySettingsMap[family_name];
    var page = await testRunner.createPage();
    await page.loadHTML(`
      <html>
      <meta charset="UTF-8">
      <style>
        .test {
          font-family: ${family_name};
        }
      </style>
      <script>
        internals.settings["set${setting_name}FontFamily"].
          call(internals.settings, "Ahem", "Zyyy");
      </script>
      <div class="test">
        <div id="${family_name}">
          This text uses the ${setting_name} font setting.
        </div>
      </div>
      </html>
    `);
    var session = await page.createSession();

    var helper = await testRunner.loadScript('./resources/layout-font-test.js');
    await helper(testRunner, session);
  }

  testRunner.completeTest();
})
