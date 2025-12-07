(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {dp} = await testRunner.startBlank('Tests CSS.getEnvironmentVariables');

  await dp.DOM.enable();
  await dp.CSS.enable();

  const stabilizeNames = [
    'keyboard-inset-bottom',
    'keyboard-inset-height',
    'keyboard-inset-left',
    'keyboard-inset-right',
    'keyboard-inset-top',
    'keyboard-inset-width',
    'preferred-text-scale',
    'safe-area-inset-bottom',
    'safe-area-inset-left',
    'safe-area-inset-right',
    'safe-area-inset-top',
    'safe-area-max-inset-bottom',
    'safe-area-max-inset-left',
    'safe-area-max-inset-right',
    'safe-area-max-inset-top',
    ...TestRunner.stabilizeNames,
  ];

  testRunner.log(
      await dp.CSS.getEnvironmentVariables(), 'env(): ', stabilizeNames);
  testRunner.completeTest();
});
