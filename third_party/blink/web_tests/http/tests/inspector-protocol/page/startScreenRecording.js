(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startBlank('Tests Page.startScreenRecording.');
  const streamHelper =
      await testRunner.loadScript('./resources/stream-helper.js');
  testRunner.log(await dp.Page.startScreenRecording(), undefined, undefined,
                 ['stream']);
  const result = await dp.Page.stopScreenRecording();
  const stream = result.result.stream;
  testRunner.log(stream ? 'stream handle successfully created' :
                          'stream handle missing');
  if (stream) {
    const decoded = await streamHelper.readStream(dp, stream);
    streamHelper.assertMp4Stream(testRunner, decoded);
  }
  testRunner.completeTest();
})
