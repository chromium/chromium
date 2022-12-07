function pictureInPictureOverflowItem(video) {
  return overflowItem(video, '-internal-media-controls-picture-in-picture-button');
}

function isPictureInPictureButtonEnabled(video) {
  var button = pictureInPictureOverflowItem(video);
  return !button.disabled && button.style.display != "none";
}

function clickPictureInPictureButton(video, callback) {
  openOverflowAndClickButton(video, pictureInPictureOverflowItem(video), callback);
}

function checkPictureInPictureInterstitialDoesNotExist(video) {
  var controlID = '-internal-picture-in-picture-interstitial-message';

  var interstitial = getElementByPseudoId(internals.shadowRoot(video), controlID);
  if (interstitial)
    throw 'Should not have a picture in picture interstitial';
}

function isPictureInPictureInterstitialVisible(video) {
  return isVisible(pictureInPictureInterstitial(video));
}

function pictureInPictureInterstitial(video)
{
  var elementId = '-internal-media-interstitial';
  var interstitial = mediaControlsElement(
      internals.shadowRoot(video).firstChild,
      elementId);
  if (!interstitial)
    throw 'Failed to find picture in picture interstitial.';
  return interstitial;
}

function pictureInPictureInterstitialMessage(video) {
  var elementId = '-internal-picture-in-picture-interstitial-message';
  var interstitial = mediaControlsElement(
      internals.shadowRoot(video).firstChild,
      elementId);
  if (!interstitial)
    throw 'Failed to find picture in picture interstitial message.';
  return interstitial;
}
