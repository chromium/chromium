// We expect the items in the overflow to appear in the following ordering.
var overflowButtonsCSS = [
    "-webkit-media-controls-play-button",
    "-webkit-media-controls-fullscreen-button",
    "-internal-media-controls-download-button",
    "-webkit-media-controls-mute-button",
    "-internal-media-controls-cast-button",
    "-webkit-media-controls-toggle-closed-captions-button",
    "-internal-media-controls-picture-in-picture-button",
];
//  PseudoID for the overflow button
var menuID = "-internal-media-controls-overflow-button";
//  PseudoID for the overflow list
var listID = "-internal-media-controls-overflow-menu-list";

// Returns the overflow menu button within the given media element
function getOverflowMenuButton(media) {
  return mediaControlsElement(internals.shadowRoot(media).firstChild, menuID);
}

// Returns the overflow menu list of overflow controls
function getOverflowList(media) {
  return mediaControlsElement(internals.shadowRoot(media).firstChild, listID);
}

// Location of media control element in the overflow button
var OverflowMenuButtons = {
  PLAY: 0,
  FULLSCREEN: 1,
  DOWNLOAD: 2,
  MUTE: 3,
  CAST: 4,
  CLOSED_CAPTIONS: 5,
};

// Default text within the overflow menu
var overflowMenuText = ["Play", "Fullscreen", "Download", "Mute", "Cast", "CaptionsOff"];

if (document.pictureInPictureEnabled)
  overflowMenuText.push('Picture in Picture');
