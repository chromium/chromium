(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Tests Page.startScreenRecording during navigation.');

  const streamHelper =
      await testRunner.loadScript('./resources/stream-helper.js');

  testRunner.log(await dp.Page.startScreenRecording({
    audio: true,
    maxWidth: 100,
    maxHeight: 100,
    frameRate: 10,
  }),
                 undefined, undefined, ['stream']);

  testRunner.log('Navigating to another page');
  testRunner.log(
      await dp.Page.navigate({url: testRunner.url('./resources/blank.html')}));

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
