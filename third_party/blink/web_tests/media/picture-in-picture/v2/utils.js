function enablePictureInPictureV2ForTest(t) {
  const pictureInPictureEnabledValue =
      internals.runtimeFlags.pictureInPictureEnabled;
  const pictureInPictureEnabledV2Value =
      internals.runtimeFlags.pictureInPictureV2Enabled;

  internals.runtimeFlags.pictureInPictureEnabled = true;
  internals.runtimeFlags.pictureInPictureV2Enabled = true;

  t.add_cleanup(() => {
    internals.runtimeFlags.pictureInPictureEnabled =
        pictureInPictureEnabledValue;
    internals.runtimeFlags.pictureInPictureV2Enabled =
        pictureInPictureEnabledV2Value;
  });
}

// Calls requestPictureInPicture() in a context that's 'allowed to request PiP'.
async function requestPictureInPictureWithTrustedClick(element, options) {
  await test_driver.bless('request Picture-in-Picture');
  return element.requestPictureInPicture(options);
}
