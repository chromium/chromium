(async function(testRunner) {
  const target1 = await testRunner.startBlank(
      'Tests Page.startScreenRecording on multiple pages - Page 1.');
  const target2 = await testRunner.startBlank(
      'Tests Page.startScreenRecording on multiple pages - Page 2.');

  const streamHelper =
      await testRunner.loadScript('./resources/stream-helper.js');

  testRunner.log('Starting recording on Page 1');
  testRunner.log(await target1.dp.Page.startScreenRecording(), undefined,
                 undefined, ['stream']);

  testRunner.log('Starting recording on Page 2');
  testRunner.log(await target2.dp.Page.startScreenRecording(), undefined,
                 undefined, ['stream']);

  async function checkStream(dp) {
    const result = await dp.Page.stopScreenRecording();

    const stream = result.result.stream;
    testRunner.log(stream ? 'stream handle successfully created' :
                            'stream handle missing');
    if (stream) {
      const decoded = await streamHelper.readStream(dp, stream);
      streamHelper.assertMp4Stream(testRunner, decoded);
    }
  }

  testRunner.log('Checking stream for Page 1');
  await checkStream(target1.dp);

  testRunner.log('Checking stream for Page 2');
  await checkStream(target2.dp);

  testRunner.completeTest();
})
