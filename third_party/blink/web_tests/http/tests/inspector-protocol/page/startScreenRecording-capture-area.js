(async function(testRunner) {
  const {dp} = await testRunner.startBlank(
      'Tests the captureArea parameter of Page.startScreenRecording.');

  testRunner.log('Unknown capture area:');
  testRunner.log(
      (await dp.Page.startScreenRecording({captureArea: 'foo'})).error);

  // content_shell has no browser window, so it cannot record it.
  testRunner.log('Browser window:');
  testRunner.log((await dp.Page.startScreenRecording({
                   captureArea: 'browserWindow'
                 })).error);

  testRunner.log('Target:');
  const {result} = await dp.Page.startScreenRecording({captureArea: 'target'});
  testRunner.log(result.stream ? 'stream handle successfully created' :
                                 'stream handle missing');
  await dp.Page.stopScreenRecording();

  testRunner.completeTest();
})
