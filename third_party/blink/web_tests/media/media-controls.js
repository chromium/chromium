var captionsButtonElement;
var captionsButtonCoordinates;

// As specified in mediaControls.css, this is how long it takes to fade out controls
const controlsFadeOutDurationMs = 300;

// The timeout for the hide-after-no-mouse-movement behavior. Defined (and
// should mirror) the value 'timeWithoutMouseMovementBeforeHidingMediaControls'
// in MediaControls.cpp.
const controlsMouseMovementTimeoutMs = 2500;

function isControlVisible(control) {
    var style = getComputedStyle(control);
    var visibility = style.getPropertyValue("visibility");
    var display = style.getPropertyValue("display");
    return (display != "none" && visibility == "visible");
}

function mediaControls(videoElement) {
  var controlID = '-webkit-media-controls';
  var element = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
  if (!element)
    throw 'Failed to find media controls';
  return element;
}

function buttonPanelElement(videoElement) {
  var controlID = '-internal-media-controls-button-panel';
  var element = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
  if (!element)
    throw 'Failed to find media controls button panel';
  return element;
}

function loadingPanelElement(videoElement) {
  var controlID = '-internal-media-controls-loading-panel';
  var element = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
  if (!element)
    throw 'Failed to find loading panel';
  return element;
}

function panelElement(videoElement) {
  var element = mediaControlsButton(videoElement, "panel");
  if (!element)
    throw 'Failed to find media controls panel';
  return element;
}

function castButton(videoElement) {
    var controlID = '-internal-media-controls-cast-button';
    var button = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find cast button';
    return button;
}

function currentTimeElement(videoElement) {
  var controlID = '-webkit-media-controls-current-time-display';
  var element = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
  if (!element)
    throw 'Failed to find current time element';
  return element;
}

function timeRemainingElement(videoElement) {
  var controlID = '-webkit-media-controls-time-remaining-display';
  var element = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
  if (!element)
    throw 'Failed to find time remaining element';
  return element;
}

function downloadButton(videoElement) {
    var controlID = '-internal-media-controls-download-button';
    var button = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find download button';
    return button;
}

function pictureInPictureButton(videoElement) {
  var controlID = '-internal-media-controls-picture-in-picture-button';
  var button = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
  if (!button)
    throw 'Failed to find picture-in-picture button';
  return button;
}

function fullscreenButton(videoElement) {
    var controlID = '-webkit-media-controls-fullscreen-button';
    var button = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find fullscreen button';
    return button;
}

function overlayCastButton(videoElement)
{
    var controlID = '-internal-media-controls-overlay-cast-button';
    var button = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find cast button';
    return button;
}

function overlayEnclosureElement(videoElement) {
    var controlID = '-webkit-media-controls-overlay-enclosure';
    var element = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
    if (!element)
        throw 'Failed to find overlay enclosure';
    return element;
}

function overflowButton(videoElement)
{
    var controlID = '-internal-media-controls-overflow-button';
    var button = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find overflow button';
    return button;
}

function textTrackMenu(video)
{
  var controlID = '-internal-media-controls-text-track-list';
  var element = mediaControlsElement(internals.shadowRoot(video).firstChild, controlID);
  if (!element)
    throw 'Failed to find the overflow menu';
  return element;
}

function textTrackListHeader(video)
{
  var element = textTrackMenu(video).childNodes[0];
  if (!element)
    throw 'Failed to find the track list header'
  return element;
}

function overflowMenu(video)
{
  var controlID = '-internal-media-controls-overflow-menu-list';
  var element = mediaControlsElement(internals.shadowRoot(video).firstChild, controlID);
  if (!element)
    throw 'Failed to find the overflow menu';
  return element;
}

function overflowItem(video, controlID) {
  var element = mediaControlsElement(overflowMenu(video).firstChild, controlID);
  if (!element)
    throw 'Failed to find overflow item: ' + controlID;
  return element;
}

function fullscreenOverflowItem(video) {
  return overflowItem(video, '-webkit-media-controls-fullscreen-button');
}

function muteOverflowItem(video) {
  return overflowItem(video, '-webkit-media-controls-mute-button');
}

function captionsOverflowItem(video) {
  return overflowItem(video, '-webkit-media-controls-toggle-closed-captions-button');
}

function castOverflowItem(video) {
  return overflowItem(video, '-internal-media-controls-cast-button');
}

function downloadsOverflowItem(video) {
  return overflowItem(video, '-internal-media-controls-download-button');
}

function mediaControlsElement(first, id)
{
    for (var element = first; element; element = element.nextSibling) {
        // Not every element in the media controls has a shadow pseudo ID, eg. the
        // text nodes for the time values, so guard against exceptions.
        try {
            if (internals.shadowPseudoId(element) == id)
                return element;
        } catch (exception) { }

        if (element.firstChild) {
            var childElement = mediaControlsElement(element.firstChild, id);
            if (childElement)
                return childElement;
        }
    }

    return null;
}

function getFocusedElement(video) {
  return internals.shadowRoot(video).activeElement;
}

function mediaControlsButton(element, id)
{
    var controlID = "-webkit-media-controls-" + id;
    var button = mediaControlsElement(internals.shadowRoot(element).firstChild, controlID);
    if (!button)
        throw "Failed to find media control element ID '" + id + "'";
    return button;
}

function elementCoordinates(element)
{
    var elementBoundingRect = element.getBoundingClientRect();
    var x = elementBoundingRect.left + elementBoundingRect.width / 2;
    var y = elementBoundingRect.top + elementBoundingRect.height / 2;
    return new Array(x, y);
}

function coordinatesOutsideElement(element)
{
    var elementBoundingRect = element.getBoundingClientRect();
    var x = elementBoundingRect.left - 1;
    var y = elementBoundingRect.top - 1;
    return new Array(x, y);
}

function videoLeftEdgeCoordinates(element) {
    const rect = element.getBoundingClientRect();
    return [rect.left + 5, rect.top + rect.height / 2];
}

function videoRightEdgeCoordinates(element) {
    const rect = element.getBoundingClientRect();
    return [rect.right - 5, rect.top + rect.height / 2];
}

function mediaControlsButtonCoordinates(element, id)
{
    var button = mediaControlsButton(element, id);
    return elementCoordinates(button);
}

function mediaControlsButtonDimensions(element, id)
{
    var button = mediaControlsButton(element, id);
    var buttonBoundingRect = button.getBoundingClientRect();
    return new Array(buttonBoundingRect.width, buttonBoundingRect.height);
}

function textTrackContainerElement(parentElement) {
    return mediaControlsElement(internals.shadowRoot(parentElement).firstChild,
        "-webkit-media-text-track-container");
}

function textTrackCueElementByIndex(parentElement, cueIndex) {
    var displayElement = textTrackDisplayElement(parentElement);
    if (displayElement) {
        for (i = 0; i < cueIndex; i++)
            displayElement = displayElement.nextSibling;
    }

    return displayElement;
}

function textTrackRegionElement(parentElement)
{
    var containerElement = textTrackContainerElement(parentElement);
    return mediaControlsElement(containerElement.firstChild, "-webkit-media-text-track-region");
}

function textTrackRegionContainerElement(parentElement)
{
    var containerElement = textTrackContainerElement(parentElement);
    return mediaControlsElement(containerElement.firstChild, "-webkit-media-text-track-region-container");
}

function textTrackDisplayElement(parentElement)
{
    var containerElement = textTrackContainerElement(parentElement);
    return mediaControlsElement(containerElement.firstChild, "-webkit-media-text-track-display");
}

function isCastButtonEnabled(video) {
  var button = castOverflowItem(video);
  return !button.disabled && button.style.display != "none";
}

function isClosedCaptionsButtonEnabled(video) {
  var button = captionsOverflowItem(video);
  return !button.disabled && button.style.display != "none";
}

function isDownloadsButtonEnabled(video) {
  var button = downloadsOverflowItem(video);
  return !button.disabled && button.style.display != "none";
}

function isFullscreenButtonEnabled(video) {
  var button = fullscreenButton(video);
  var overflowButton = fullscreenOverflowItem(video);
  return (!button.disabled && button.style.display != "none") ||
      (!overflowButton.disabled && overflowButton.style.display != "none");
}

function isClosedCaptionsButtonVisible(currentMediaElement)
{
    var captionsButtonElement = mediaControlsButton(currentMediaElement, "toggle-closed-captions-button");
    var captionsButtonCoordinates = mediaControlsButtonCoordinates(currentMediaElement, "toggle-closed-captions-button");

    if (!captionsButtonElement.disabled
        && captionsButtonCoordinates[0] > 0
        && captionsButtonCoordinates[1] > 0) {
        return true;
    }

    return false;
}

function toggleClosedCaptionsButton(videoElement) {
    return mediaControlsButton(videoElement, 'toggle-closed-captions-button');
}

function playButton(videoElement) {
    return mediaControlsButton(videoElement, 'play-button');
}

function enabledPlayButton(videoElement) {
  if (internals.runtimeFlags.mediaControlsOverlayPlayButtonEnabled) {
    return mediaControlsOverlayPlayButton(videoElement);
  }
  return playButton(videoElement);
}

function muteButton(videoElement) {
    return mediaControlsButton(videoElement, 'mute-button');
}

function volumeSliderElement(videoElement) {
  return mediaControlsButton(videoElement, 'volume-slider');
}

function isVolumeSliderOpen(videoElement) {
  return !volumeSliderElement(videoElement).classList.contains('closed');
}

function runAfterVolumeSliderAnimationEnds(func) {
  // 300ms timer plus 200ms slack.
  const volumeSliderTimeoutMs = 300 + 200;
  setTimeout(func, volumeSliderTimeoutMs);
}

function timelineElement(videoElement) {
    return mediaControlsButton(videoElement, 'timeline');
}

function timelineThumb(videoElement) {
    const timeline = timelineElement(videoElement);
    const thumb = internals.shadowRoot(timeline).getElementById('thumb');
    if (!thumb)
        throw 'Failed to find timeline thumb';
    return thumb;
}

function scrubbingMessageElement(videoElement) {
    var controlID = '-internal-media-controls-scrubbing-message';
    var button = mediaControlsElement(internals.shadowRoot(videoElement).firstChild, controlID);
    if (!button)
        throw 'Failed to find scrubbing message element';
    return button;
}

function clickAtCoordinates(x, y) {
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function openOverflowAndClickButton(video, button, callback) {
  singleTapOnControl(overflowButton(video), function () {
    singleTapOnControl(button, callback);
  });
}

function clickDownloadButton(video, callback) {
  openOverflowAndClickButton(video, downloadsOverflowItem(video), callback);
}

function textTrackListItemAtIndex(video, index) {
    var trackListItems = textTrackMenu(video).childNodes;
    for (var i = 0; i < trackListItems.length; i++) {
        var trackListItem = trackListItems[i];
        var innerCheckbox = textTrackListItemInnerCheckbox(trackListItem);
        if (innerCheckbox && innerCheckbox.getAttribute("data-track-index") == index)
            return trackListItem;
    }
}

function textTrackListItemInnerCheckbox(trackListItem) {
  const children = trackListItem.children;
  for (var i = 0; i < children.length; i++) {
    const child = children[i];
    if (internals.shadowPseudoId(child) == "-internal-media-controls-text-track-list-item-input")
      return child;
  }
  return null;
}

function textTrackListItemInnerKindIndicator(trackListItem) {
  const children = trackListItem.children;
  for (var i = 0; i < children.length; i++) {
    const child = children[i];
    const pseudoId = internals.shadowPseudoId(child);
    if (pseudoId == "-internal-media-controls-text-track-list-kind-captions" ||
        pseudoId == "-internal-media-controls-text-track-list-kind-subtitles") {
      return child;
    }
  }
  return null;
}

function clickCaptionButton(video, callback) {
  openOverflowAndClickButton(video, captionsOverflowItem(video), callback);
}

function clickTextTrackAtIndex(video, index, callback) {
    clickCaptionButton(video, function () {
      var track = textTrackListItemAtIndex(video, index);
      track.scrollIntoView();
      singleTapOnControl(track, callback);
    });
}

function turnClosedCaptionsOff(video, callback)
{
    clickTextTrackAtIndex(video, -1, callback);
}

function checkCaptionsVisible(video, captions)
{
    for (var i = 0; i < captions.length; i++) {
      assert_equals(textTrackCueElementByIndex(video, i).innerText, captions[i]);
    }
}

function checkCaptionsHidden(video)
{
    assert_equals(textTrackDisplayElement(video), null);
}

function runAfterHideMediaControlsTimerFired(func, mediaElement)
{
    if (mediaElement.paused)
        throw "The media element is not playing";

    // Compute the time it'll take until the controls will be invisible -
    // assuming playback has been started prior to invoking this
    // function. Allow 500ms slack.
    var hideTimeoutMs = controlsMouseMovementTimeoutMs + controlsFadeOutDurationMs + 500;

    if (!mediaElement.loop && hideTimeoutMs >= 1000 * (mediaElement.duration - mediaElement.currentTime))
        throw "The media will end before the controls have been hidden";

    setTimeout(func, hideTimeoutMs);
}

function runAfterDoubleTapTimerFired(func) {
  // 300ms timer plus 500ms slack.
  const doubleTapTimeoutMs = 300 + 500;
  setTimeout(func, doubleTapTimeoutMs);
}

// Requests an animation frame.
function waitForHoverEffectUpdate(func) {
  // The hover effect is updated at the next animation frame after the layout
  // changes.
  return new Promise((resolve) => {
    requestAnimationFrame(() => {
      resolve();
    });
  });
}

function hoverMuteButton(video, func) {
  // Wait for hover timer fires
  const delayedCallback = function() { setTimeout(func); };
  const muteBtn = muteButton(video);
  hoverOverControl(muteBtn, delayedCallback);
}

function hasEnabledFullscreenButton(element) {
  var button = fullscreenButton(element);
  return !button.disabled && button.style.display != "none";
}

function isControlsPanelVisible(element)
{
    return getComputedStyle(mediaControlsButton(element, "panel")).opacity == "1";
}

function isVisible(button) {
    var computedStyle = getComputedStyle(button);
    return computedStyle.display !== "none" &&
           computedStyle.visibility === "visible";
}

function checkButtonHasClass(button, className) {
  assert_true(button.classList.contains(className));
}

function checkButtonNotHasClass(button, className) {
  assert_false(button.classList.contains(className));
}

function checkControlsClassName(videoElement, className) {
  assert_equals(internals.shadowRoot(videoElement).firstChild.className, className);
}

function checkControlsHasClass(videoElement, className) {
  assert_true(mediaControls(videoElement).classList.contains(className),
      'Controls should have class: ' + className);
}

function checkControlsDoesNotHaveClass(videoElement, className) {
  assert_false(mediaControls(videoElement).classList.contains(className),
      'Controls should not have class: ' + className);
}

function checkControlsHasClasses(videoElement, classes) {
  classes.forEach(className => {
    checkControlsHasClass(videoElement, className);
  });
}

function checkControlsDoesNotHaveClasses(videoElement, classes) {
  classes.forEach(className => {
    checkControlsDoesNotHaveClass(videoElement, className);
  });
}

function mediaControlsOverlayPlayButton(videoElement) {
  return mediaControlsButton(videoElement, 'overlay-play-button');
}

function mediaControlsOverlayPlayButtonInternal(videoElement) {
  var controlID = '-internal-media-controls-overlay-play-button-internal';
  var element = mediaControlsElement(
      internals.shadowRoot(
          mediaControlsOverlayPlayButton(videoElement)).firstChild, controlID);
  if (!element)
    throw 'Failed to find the internal overlay play button';
  return element;
}

function pictureInPictureInterstitial(videoElement) {
  var controlID = '-internal-picture-in-picture-interstitial-message';

  var interstitial = getElementByPseudoId(internals.shadowRoot(videoElement).firstChild, controlID);
  if (!interstitial)
      throw 'Failed to find picture in picture interstitial';
  return interstitial;
}

function checkPictureInPictureInterstitialDoesNotExist(videoElement) {
  var controlID = '-internal-picture-in-picture-interstitial-message';

  var interstitial = getElementByPseudoId(internals.shadowRoot(videoElement), controlID);
  if (interstitial)
      throw 'Should not have a picture in picture interstitial';
}

function doubleTapAtCoordinates(x, y, timeout, callback) {
  timeout = timeout == undefined ? 100 : timeout;

  chrome.gpuBenchmarking.pointerActionSequence([
    {
      source: 'mouse',
      actions: [
        { name: 'pointerDown', x: x, y: y },
        { name: 'pointerUp' },
        { name: 'pause', duration: timeout },
        { name: 'pointerDown', x: x, y: y },
        { name: 'pointerUp' }
      ]
    }
  ], callback);
}

function singleTapAtCoordinates(xPos, yPos, callback) {
  chrome.gpuBenchmarking.pointerActionSequence([
    {
      source: 'mouse',
      actions: [
        { name: 'pointerDown', x: xPos, y: yPos },
        { name: 'pointerUp' }
      ]
    }
  ], callback);
}

function singleTapOutsideControl(control, callback) {
  const coordinates = coordinatesOutsideElement(control);
  singleTapAtCoordinates(coordinates[0], coordinates[1], callback);
}

function singleTapOnControl(control, callback) {
  const coordinates = elementCoordinates(control);
  singleTapAtCoordinates(coordinates[0], coordinates[1], callback);
}

function hoverOverControl(control, callback) {
  const coordinates = elementCoordinates(control);
  chrome.gpuBenchmarking.pointerActionSequence([
    {
      source: 'mouse',
      actions: [
        { name: 'pointerMove', x: coordinates[0], y: coordinates[1] }
      ]
    }
  ], callback);
}

// This function does not work on Mac due to crbug.com/613672. When using this
// function, add an entry into TestExpectations to skip on Mac.
function singleTouchAtCoordinates(xPos, yPos, callback) {
  chrome.gpuBenchmarking.pointerActionSequence([
    {
      source: 'touch',
      actions: [
        { name: 'pointerDown', x: xPos, y: yPos },
        { name: 'pointerUp' }
      ]
    }
  ], callback);
}

function doubleTouchAtCoordinates(x, y, timeout, callback) {
  timeout = timeout == undefined ? 100 : timeout;

  chrome.gpuBenchmarking.pointerActionSequence([
    {
      source: 'touch',
      actions: [
        { name: 'pointerDown', x: x, y: y },
        { name: 'pointerUp' },
        { name: 'pause', duration: timeout },
        { name: 'pointerDown', x: x, y: y },
        { name: 'pointerUp' }
      ]
    }
  ], callback);
}

function singleTouchOnControl(control, callback) {
  const coordinates = elementCoordinates(control);
  singleTouchAtCoordinates(coordinates[0], coordinates[1], callback);
}

function traverseNextNode(node, stayWithin) {
    var nextNode = node.firstChild;
    if (nextNode)
        return nextNode;

    if (stayWithin && node === stayWithin)
        return null;

    nextNode = node.nextSibling;
    if (nextNode)
        return nextNode;

    nextNode = node;
    while (nextNode && !nextNode.nextSibling && (!stayWithin || !nextNode.parentNode || nextNode.parentNode !== stayWithin))
        nextNode = nextNode.parentNode;
    if (!nextNode)
        return null;

    return nextNode.nextSibling;
}

function getElementByPseudoId(root, pseudoId) {
    if (!window.internals)
        return null;
    var node = root;
    while (node) {
        if (node.nodeType === Node.ELEMENT_NODE && internals.shadowPseudoId(node) === pseudoId)
            return node;
        node = traverseNextNode(node, root);
    }
    return null;
}

function enableTestMode(video) {
  if (window.internals)
    internals.setMediaControlsTestMode(video, true);
}

function enableImmersiveMode(t) {
  if (!window.internals)
    return;

  const oldImmersive = internals.settings.immersiveModeEnabled;
  internals.settings.setImmersiveModeEnabled(true);
  t.add_cleanup(() => {
    internals.settings.setImmersiveModeEnabled(oldImmersive);
  });
}

function setPreferHiddenVolumeControlsForTest(t, preferHidden) {
  if (!window.internals)
    return;

  const oldSetting = internals.settings.preferHiddenVolumeControls;
  internals.settings.setPreferHiddenVolumeControls(preferHidden);
  t.add_cleanup(() => {
    internals.settings.setPreferHiddenVolumeControls(oldSetting);
  });
}
