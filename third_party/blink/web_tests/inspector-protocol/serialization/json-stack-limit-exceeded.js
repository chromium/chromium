(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that serialization of deeply nested objects results in an error`);

  await session.evaluate(() => {
    function add(root = document.body, level = 0) {
      if (level > 300) {
        return;
      }
      const div = document.createElement('div');
      root.append(div);
      add(div, level + 1);
    }
    add();
  });

  testRunner.log(await dp.DOM.getDocument({depth:-1}));
  testRunner.completeTest();
});
