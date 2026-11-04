(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests that Page.startScreenRecording rejects invalid parameters.');

  const cases = [
    {maxWidth: 0},
    {maxHeight: -1},
    {maxWidth: 40000},
    {maxWidth: 30000, maxHeight: 30000},
    {frameRate: 1001},
  ];
  for (const params of cases) {
    testRunner.log(`Parameters: ${JSON.stringify(params)}`);
    testRunner.log((await dp.Page.startScreenRecording(params)).error);
  }

  testRunner.log('Valid parameters:');
  const {result} = await dp.Page.startScreenRecording(
      {maxWidth: 640, maxHeight: 480, frameRate: 30});
  testRunner.log(result.stream ? 'stream handle successfully created' :
                                 'stream handle missing');
  await dp.Page.stopScreenRecording();

  testRunner.completeTest();
})
