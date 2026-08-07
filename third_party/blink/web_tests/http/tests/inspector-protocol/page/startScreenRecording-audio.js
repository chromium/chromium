(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests Page.startScreenRecording audio options.');

  const streamHelper =
      await testRunner.loadScript('./resources/stream-helper.js');

  async function testRecording(audio) {
    testRunner.log(`\nTesting with audio: ${audio}`);
    testRunner.log(await dp.Page.startScreenRecording({
      audio: audio,
      maxWidth: 100,
      maxHeight: 100,
      frameRate: 10,
    }),
                   undefined, undefined, ['stream']);

    const result = await dp.Page.stopScreenRecording();

    const stream = result.result.stream;
    testRunner.log(stream ? 'stream handle successfully created' :
                            'stream handle missing');
    if (stream) {
      const decoded = await streamHelper.readStream(dp, stream);
      streamHelper.assertMp4Stream(testRunner, decoded);
    }
  }

  await testRecording(true);
  await testRecording(false);

  testRunner.completeTest();
})
