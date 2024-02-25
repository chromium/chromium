// META: title=Test configurationchange event
// META: script=/mediacapture-image/resources/imagecapture-helpers.js

image_capture_test(async (t, imageCaptureTest) => {
  imageCaptureTest.mockImageCapture().turnOffBackgroundBlurMode();

  const stream = await navigator.mediaDevices.getUserMedia({video: true});
  t.add_cleanup(() => stream.getTracks()[0].stop());

  const track = stream.getVideoTracks()[0];
  const configurationChangePromise = new Promise(
      resolve => track.addEventListener('configurationchange', resolve));
  assert_equals(track.getSettings().backgroundBlur, false);

  imageCaptureTest.mockImageCapture().turnOnBackgroundBlurMode();
  internals.fakeCaptureConfigurationChanged(track);
  await configurationChangePromise;
  assert_equals(track.getSettings().backgroundBlur, true);
}, 'configurationchange event is fired when background blur mode has changed');

image_capture_test(async (t, imageCaptureTest) => {
  imageCaptureTest.mockImageCapture().turnOffSupportedBackgroundBlurModes();

  const stream = await navigator.mediaDevices.getUserMedia({video: true});
  t.add_cleanup(() => stream.getTracks()[0].stop());

  const track = stream.getVideoTracks()[0];
  const configurationChangePromise = new Promise(
      resolve => track.addEventListener('configurationchange', resolve));
  assert_array_equals(track.getCapabilities().backgroundBlur, [false]);

  imageCaptureTest.mockImageCapture().turnOnSupportedBackgroundBlurModes();
  internals.fakeCaptureConfigurationChanged(track);
  await configurationChangePromise;
  assert_array_equals(track.getCapabilities().backgroundBlur, [true]);
}, 'configurationchange event is fired when supported background blur modes have changed');

image_capture_test(async (t, imageCaptureTest) => {
  const stream = await navigator.mediaDevices.getUserMedia({video: true});
  t.add_cleanup(() => stream.getTracks()[0].stop());

  const track = stream.getVideoTracks()[0];
  track.addEventListener('configurationchange', () => {
    assert_unreached('expected configurationchange to not fire');
  });

  internals.fakeCaptureConfigurationChanged(track);
  await new Promise(resolve => setTimeout(resolve, 0));
}, 'configurationchange event is not fired when nothing has changed');

image_capture_test(async (t, imageCaptureTest) => {
  imageCaptureTest.mockImageCapture().turnOffBackgroundBlurMode();

  const stream = await navigator.mediaDevices.getUserMedia({video: true});
  t.add_cleanup(() => stream.getTracks()[0].stop());

  const track = stream.getVideoTracks()[0];
  track.addEventListener('configurationchange', () => {
    assert_unreached('expected configurationchange to not fire');
  });

  track.stop();
  imageCaptureTest.mockImageCapture().turnOnBackgroundBlurMode();
  internals.fakeCaptureConfigurationChanged(track);
  await new Promise(resolve => setTimeout(resolve, 0));
}, 'configurationchange event is not fired when track is ended');
