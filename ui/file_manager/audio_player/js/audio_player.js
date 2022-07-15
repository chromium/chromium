// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../elements/audio_player.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {dashToCamelCase} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {appUtil} from '../../file_manager/common/js/app_util.js';
import {AsyncUtil} from '../../file_manager/common/js/async_util.js';
import {FilteredVolumeManager} from '../../file_manager/common/js/filtered_volume_manager.js';
import {MediaSessionPlaybackState} from '../../file_manager/common/js/mediasession_types.js';
import {util} from '../../file_manager/common/js/util.js';
import {AllowedPaths} from '../../file_manager/common/js/volume_manager_types.js';
import {ExternallyUnmountedEvent} from '../../file_manager/externs/volume_manager.js';
import {ContentMetadataProvider} from '../../file_manager/foreground/js/metadata/content_metadata_provider.js';
import {MetadataModel} from '../../file_manager/foreground/js/metadata/metadata_model.js';

/**
 * @param {Element} container Container element.
 * @constructor
 */
export function AudioPlayer(container) {
  this.container_ = container;

  this.volumeManager_ = new FilteredVolumeManager(
      AllowedPaths.ANY_PATH, false, appUtil.getVolumeManager(), []);

  this.resolveMetadataModel_ = null;
  this.metadataModelReady_ = new Promise(resolve => {
    this.resolveMetadataModel_ = resolve;
  });
  this.metadataModel_ = null;

  this.selectedEntry_ = null;
  this.invalidTracks_ = {};
  this.entries_ = [];
  this.currentTrackIndex_ = -1;
  this.playlistGeneration_ = 0;

  /**
   * Whether if the playlist is expanded or not. This value is changed by
   * this.syncPlaylistExpanded().
   * True: expanded, false: collapsed, null: unset.
   *
   * @type {?boolean}
   * @private
   */
  // Initial value is null. It'll be set in load().
  this.isPlaylistExpanded_ = null;

  /**
   * Whether if the trackinfo is expanded or not.
   * True: expanded, false: collapsed, null: unset.
   *
   * @type {?boolean}
   * @private
   */
  // Initial value is null. It'll be set in load().
  this.isTrackInfoExpanded_ = null;

  this.player_ =
    /** @type {AudioPlayerElement} */ (document.querySelector('audio-player'));
  this.player_.tracks = [];
  this.isRtl_ = window.getComputedStyle(this.player_)['direction'] === 'rtl';

  /**
   * Queue to throttle concurrent reading of audio file metadata.
   * Here we loads up to 25 songs concurrently to cover the number of songs in
   * an album in most cases. This number should not be too large so that the
   * number of open file descriptors will not hit the system limit.
   * @private {AsyncUtil.ConcurrentQueue}
   */
  this.loadMetadataQueue_ = new AsyncUtil.ConcurrentQueue(25);

  // Restore the saved state from local storage, and update the local storage
  // if the states are changed.
  const STORAGE_PREFIX = 'audioplayer-';
  const KEYS_TO_SAVE_STATES = [
    'shuffle',
    'repeat-mode',
    'volume',
    'playlist-expanded',
    'track-info-expanded',
  ];
  const storageKeys = KEYS_TO_SAVE_STATES.map(a => STORAGE_PREFIX + a);
  chrome.storage.local.get(storageKeys, function(results) {
    // Update the UI by loaded state.
    for (const storageKey in results) {
      const key = storageKey.substr(STORAGE_PREFIX.length);
      this.player_[dashToCamelCase(key)] = results[storageKey];
    }
    // Start listening to UI changes to write back the states to local storage.
    for (let i = 0; i < KEYS_TO_SAVE_STATES.length; i++) {
      this.player_.addEventListener(
          KEYS_TO_SAVE_STATES[i] + '-changed', function(storageKey, event) {
            const objectToBeSaved = {};
            objectToBeSaved[storageKey] = event.detail.value;
            chrome.storage.local.set(objectToBeSaved);
          }.bind(this, storageKeys[i]));
    }
  }.bind(this));

  // Update the window size when UI's 'playlist-expanded' state is changed.
  this.player_.addEventListener('playlist-expanded-changed', function(event) {
    this.onPlaylistExpandedChanged_(event.detail.value);
  }.bind(this));

  // Update the window size when UI's 'track-info-expanded' state is changed.
  this.player_.addEventListener('track-info-expanded-changed', function(event) {
    this.onTrackInfoExpandedChanged_(event.detail.value);
  }.bind(this));

  this.player_.addEventListener(
      'playing-changed', this.updateMediaSessionPlaybackState_.bind(this));

  // Run asynchronously after an event of model change is delivered.
  setTimeout(function() {
    this.errorString_ = '';
    this.offlineString_ = '';
    chrome.fileManagerPrivate.getStrings(function(strings) {
      strings = /** @type {!Object<string>} */ (strings);
      loadTimeData.data = strings;

      container.ownerDocument.title = strings['AUDIO_PLAYER_TITLE'];
      this.errorString_ = strings['AUDIO_ERROR'];
      this.offlineString_ = strings['AUDIO_OFFLINE'];
      AudioPlayer.TrackInfo.DEFAULT_ARTIST =
          strings['AUDIO_PLAYER_DEFAULT_ARTIST'];
      // Pass translated labels to the AudioPlayerElement.
      this.player_.ariaLabels = {
        shuffle: strings['AUDIO_PLAYER_SHUFFLE_BUTTON_LABEL'],
        repeat: strings['AUDIO_PLAYER_REPEAT_BUTTON_LABEL'],
        previous: strings['MEDIA_PLAYER_PREVIOUS_BUTTON_LABEL'],
        play: strings['MEDIA_PLAYER_PLAY_BUTTON_LABEL'],
        pause: strings['MEDIA_PLAYER_PAUSE_BUTTON_LABEL'],
        next: strings['MEDIA_PLAYER_NEXT_BUTTON_LABEL'],
        playList: strings['AUDIO_PLAYER_OPEN_PLAY_LIST_BUTTON_LABEL'],
        seekSlider: strings['MEDIA_PLAYER_SEEK_SLIDER_LABEL'],
        mute: strings['MEDIA_PLAYER_MUTE_BUTTON_LABEL'],
        unmute: strings['MEDIA_PLAYER_UNMUTE_BUTTON_LABEL'],
        volumeSlider: strings['MEDIA_PLAYER_VOLUME_SLIDER_LABEL'],
      };
      this.player_.ariaExpandArtworkLabel =
          strings['AUDIO_PLAYER_ARTWORK_EXPAND_BUTTON_LABEL'];

      // Override metadata worker's path.
      ContentMetadataProvider.configure(
          '/js/metadata_worker.js',
          /*isModule=*/ true);

      this.metadataModel_ = MetadataModel.create(this.volumeManager_);
      this.resolveMetadataModel_();
    }.bind(this));

    this.volumeManager_.addEventListener('externally-unmounted',
        this.onExternallyUnmounted_.bind(this));

    window.addEventListener('resize', this.onResize_.bind(this));
    document.addEventListener('keydown', this.onKeyDown_.bind(this));

    // Show the window after DOM is processed.
    const currentWindow = chrome.app.window.current();
    if (currentWindow) {
      setTimeout(currentWindow.show.bind(currentWindow), 0);
    }
  }.bind(this), 0);
}

/**
 * Initial load method (static).
 */
AudioPlayer.load = function() {
  document.ondragstart = function(e) {
    e.preventDefault();
  };

  AudioPlayer.instance =
      new AudioPlayer(document.querySelector('.audio-player'));

  reload();
};

/**
 * Unloads the player.
 */
export function unload() {
  if (AudioPlayer.instance) {
    AudioPlayer.instance.onUnload();
  }
}

/**
 * Reloads the player.
 */
export function reload() {
  AudioPlayer.instance.load(/** @type {Playlist} */ (window.appState));
}

/**
 * Loads a new playlist.
 * @param {Playlist} playlist Playlist object passed via mediaPlayerPrivate.
 */
AudioPlayer.prototype.load = function(playlist) {
  this.playlistGeneration_++;
  this.currentTrackIndex_ = -1;

  // Save the app state, in case of restart. Make a copy of the object, so the
  // playlist member is not changed after entries are resolved.
  window.appState = /** @type {Playlist} */ (
      JSON.parse(JSON.stringify(playlist)));  // cloning
  appUtil.saveAppState();

  this.isPlaylistExpanded_ = this.player_.playlistExpanded;
  this.isTrackInfoExpanded_ = this.player_.trackInfoExpanded;

  // Resolving entries has to be done after the volume manager is initialized.
  this.volumeManager_.ensureInitialized(function() {
    util.URLsToEntries(playlist.items || [], function(entries) {
      this.entries_ = entries;

      const position = playlist.position || 0;

      if (this.entries_.length == 0) {
        return;
      }

      const newTracks = [];
      const currentTracks = this.player_.tracks;
      let unchanged = (currentTracks.length === this.entries_.length);

      for (let i = 0; i != this.entries_.length; i++) {
        const entry = this.entries_[i];
        newTracks.push(new AudioPlayer.TrackInfo(entry));

        if (unchanged && entry.toURL() !== currentTracks[i].url) {
          unchanged = false;
        }
      }

      if (!unchanged) {
        this.player_.tracks = newTracks;
      }

      // Run asynchronously, to makes it sure that the handler of the track list
      // is called, before the handler of the track index.
      setTimeout(function() {
        this.select_(position);

        // Load the selected track metadata first, then load the rest.
        this.loadMetadata_(position);
        for (let i = 0; i != this.entries_.length; i++) {
          if (i != position) {
            this.loadMetadata_(i);
          }
        }
      }.bind(this), 0);
    }.bind(this));
  }.bind(this));
};

/**
 * Loads metadata for a track.
 * @param {number} track Track number.
 * @private
 */
AudioPlayer.prototype.loadMetadata_ = function(track) {
  this.loadMetadataQueue_.run(function(callback) {
    this.fetchMetadata_(this.entries_[track], function(metadata) {
      this.displayMetadata_(track, metadata);
      callback();
    }.bind(this));
  }.bind(this));
};

/**
 * Displays track's metadata.
 * @param {number} track Track number.
 * @param {Object} metadata Metadata object.
 * @param {string=} opt_error Error message.
 * @private
 */
AudioPlayer.prototype.displayMetadata_ = function(track, metadata, opt_error) {
  this.player_.tracks[track].setMetadata(metadata, opt_error);
  this.player_.notifyTrackMetadataUpdated(track);
};

/**
 * Closes audio player when a volume containing the selected item is unmounted.
 * @param {Event} event The unmount event.
 * @private
 */
AudioPlayer.prototype.onExternallyUnmounted_ = function(event) {
  if (!this.selectedEntry_) {
    return;
  }

  event = /** @type {!ExternallyUnmountedEvent} */ (event);
  if (this.volumeManager_.getVolumeInfo(this.selectedEntry_) === event.detail) {
    window.close();
  }
};

/**
 * Called on window is being unloaded.
 */
AudioPlayer.prototype.onUnload = function() {
  if (this.player_) {
    this.player_.onPageUnload();
  }

  if (this.volumeManager_) {
    this.volumeManager_.dispose();
  }
};

/**
 * Selects a new track to play.
 * @param {number} newTrack New track number.
 * @private
 */
AudioPlayer.prototype.select_ = function(newTrack) {
  if (this.currentTrackIndex_ == newTrack) return;

  this.currentTrackIndex_ = newTrack;
  this.player_.currentTrackIndex = this.currentTrackIndex_;
  this.player_.time = 0;

  // Run asynchronously after an event of current track change is delivered.
  setTimeout(function() {
    if (!window.appReopen) {
      this.player_.play();
    }

    window.appState.position = this.currentTrackIndex_;
    window.appState.time = 0;
    appUtil.saveAppState();

    const entry = this.entries_[this.currentTrackIndex_];

    this.fetchMetadata_(entry, function(metadata) {
      if (this.currentTrackIndex_ != newTrack) {
        return;
      }

      this.selectedEntry_ = entry;
    }.bind(this));
  }.bind(this), 0);
};

/**
 * @param {FileEntry} entry Track file entry.
 * @param {function(Object)} callback Callback.
 * @private
 */
AudioPlayer.prototype.fetchMetadata_ = async function(entry, callback) {
  await this.metadataModelReady_;
  this.metadataModel_.get(
      [entry],
      ['mediaTitle', 'mediaArtist', 'present', 'contentThumbnailUrl']).then(
      function(generation, metadata) {
        // Do nothing if another load happened since the metadata request.
        if (this.playlistGeneration_ == generation) {
          callback(metadata[0]);
        }
      }.bind(this, this.playlistGeneration_));
};

/**
 * Media error handler.
 * @private
 */
AudioPlayer.prototype.onError_ = function() {
  const track = this.currentTrackIndex_;

  this.invalidTracks_[track] = true;

  this.fetchMetadata_(this.entries_[track], function(metadata) {
    const error = (!navigator.onLine && !metadata.present) ?
        this.offlineString_ :
        this.errorString_;
    this.displayMetadata_(track, metadata, error);
    this.player_.onAudioError();
  }.bind(this));
};

/**
 * Toggles the expanded mode when resizing.
 *
 * @param {Event} event Resize event.
 * @private
 */
AudioPlayer.prototype.onResize_ = function(event) {
  const trackListHeight =
      (/** @type {{trackList:TrackListElement}} */ (this.player_.$))
          .trackList.clientHeight;
  if (trackListHeight > AudioPlayer.TOP_PADDING_HEIGHT) {
    this.isPlaylistExpanded_ = true;
    this.player_.playlistExpanded = true;
  } else {
    this.isPlaylistExpanded_ = false;
    this.player_.playlistExpanded = false;
  }
};

/**
 * Handles keydown event to open inspector with shortcut keys.
 *
 * @param {Event} event KeyDown event.
 * @private
 */
AudioPlayer.prototype.onKeyDown_ = function(event) {
  switch (util.getKeyModifiers(event) + event.key) {
    case 'Ctrl-w': // Ctrl+W => Close the player.
    case 'BrowserBack':
      chrome.app.window.current().close();
      break;

    // Handle debug shortcut keys.
    case 'Ctrl-Shift-I': // Ctrl+Shift+I
      chrome.fileManagerPrivate.openInspector(
          chrome.fileManagerPrivate.InspectionType.NORMAL);
      break;
    case 'Ctrl-Shift-J': // Ctrl+Shift+J
      chrome.fileManagerPrivate.openInspector(
          chrome.fileManagerPrivate.InspectionType.CONSOLE);
      break;
    case 'Ctrl-Shift-C': // Ctrl+Shift+C
      chrome.fileManagerPrivate.openInspector(
          chrome.fileManagerPrivate.InspectionType.ELEMENT);
      break;
    case 'Ctrl-Shift-B': // Ctrl+Shift+B
      chrome.fileManagerPrivate.openInspector(
          chrome.fileManagerPrivate.InspectionType.BACKGROUND);
      break;

    case ' ': // Space
    case 'k':
    case 'MediaPlayPause':
      this.player_.dispatchEvent(new Event('toggle-pause-event'));
      break;
    case 'ArrowUp':
      this.player_.dispatchEvent(new Event('small-forward-skip-event'));
      break;
    case 'ArrowDown':
      this.player_.dispatchEvent(new Event('small-backword-skip-event'));
      break;
    case 'ArrowRight': {
      const eventName = this.isRtl_ ? 'small-backword-skip-event' :
                                      'small-forward-skip-event';
      this.player_.dispatchEvent(new Event(eventName));
    } break;
    case 'ArrowLeft': {
      const eventName = this.isRtl_ ? 'small-forward-skip-event' :
                                      'small-backword-skip-event';
      this.player_.dispatchEvent(new Event(eventName));
    } break;
    case 'l':
      this.player_.dispatchEvent(new Event('big-forward-skip-event'));
      break;
    case 'j':
      this.player_.dispatchEvent(new Event('big-backword-skip-event'));
      break;
    case ']':
    case 'MediaTrackNext':
      this.player_.dispatchEvent(new Event('next-track-event'));
      break;
    case '[':
    case 'MediaTrackPrevious':
      this.player_.dispatchEvent(new Event('previous-track-event'));
      break;
    case 'MediaStop':
      // TODO: Define "Stop" behavior.
      break;
  }
};

/**
 * Updates the Media Session API with the current playback state of the audio
 * player.
 * @param {Event} event The playing event.
 * @private
 */
AudioPlayer.prototype.updateMediaSessionPlaybackState_ = function(event) {
  if (!navigator.mediaSession) {
    return;
  }

  navigator.mediaSession.playbackState = event.detail.value ?
      MediaSessionPlaybackState.PLAYING :
      MediaSessionPlaybackState.PAUSED;
};

/* Keep the below constants in sync with the CSS. */

/**
 * Window header size in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.HEADER_HEIGHT = 33;  // 32px + border 1px

/**
 * Top padding height of audio player in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.TOP_PADDING_HEIGHT = 4;

/**
 * Track height in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.TRACK_HEIGHT = 48;

/**
 * artwork panel height in pixels, when it's closed.
 * @type {number}
 * @const
 */
AudioPlayer.CLOSED_ARTWORK_HEIGHT = 48;

/**
 * artwork panel height in pixels, when it's opened.
 * @type {number}
 * @const
 */
AudioPlayer.EXPANDED_ARTWORK_HEIGHT = 320;

/**
 * Controls bar height in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.CONTROLS_HEIGHT = 96;

/**
 * Default number of items in the expanded mode.
 * @type {number}
 * @const
 */
AudioPlayer.DEFAULT_EXPANDED_ITEMS = 5;

/**
 * Minimum size of the window in the expanded mode in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.EXPANDED_MODE_MIN_HEIGHT = AudioPlayer.TOP_PADDING_HEIGHT +
                                       AudioPlayer.EXPANDED_ARTWORK_HEIGHT +
                                       AudioPlayer.CONTROLS_HEIGHT;

/**
 * Minimum size of the window in the mode in pixels.
 * @type {number}
 * @const
 */
AudioPlayer.CLOSED_MODE_MIN_HEIGHT = AudioPlayer.TOP_PADDING_HEIGHT +
                                     AudioPlayer.CLOSED_ARTWORK_HEIGHT +
                                     AudioPlayer.CONTROLS_HEIGHT;

/**
 * Invoked when the 'playlist-expanded' property in the model is changed.
 * @param {boolean} newValue New value.
 * @private
 */
AudioPlayer.prototype.onPlaylistExpandedChanged_ = function(newValue) {
  if (this.isPlaylistExpanded_ !== null &&
      this.isPlaylistExpanded_ === newValue) {
    return;
  }

  if (this.isPlaylistExpanded_ && !newValue) {
    this.lastExpandedInnerHeight_ = window.innerHeight;
  }

  if (this.isPlaylistExpanded_ !== newValue) {
    this.isPlaylistExpanded_ = newValue;
    this.syncHeightForPlaylist_();

    // Saves new state.
    window.appState.playlistExpanded = newValue;
    appUtil.saveAppState();
  }
};

/**
 * Invoked when the 'track-info-expanded' property in the model is changed.
 * @param {boolean} newValue New value.
 * @private
 */
AudioPlayer.prototype.onTrackInfoExpandedChanged_ = function(newValue) {
  if (this.isTrackInfoExpanded_ !== null &&
      this.isTrackInfoExpanded_ === newValue) {
    return;
  }

  this.lastExpandedInnerHeight_ = window.innerHeight;

  if (this.isTrackInfoExpanded_ !== newValue) {
    this.isTrackInfoExpanded_ = newValue;
    const state = chrome.app.window.current();
    let newHeight = window.outerHeight;
    if (newValue) {
      state.innerBounds.minHeight = AudioPlayer.EXPANDED_MODE_MIN_HEIGHT;
      newHeight += AudioPlayer.EXPANDED_MODE_MIN_HEIGHT -
          AudioPlayer.CLOSED_MODE_MIN_HEIGHT;
    } else {
      state.innerBounds.minHeight = AudioPlayer.CLOSED_MODE_MIN_HEIGHT;
      newHeight -= AudioPlayer.EXPANDED_MODE_MIN_HEIGHT -
          AudioPlayer.CLOSED_MODE_MIN_HEIGHT;
    }
    window.resizeTo(window.outerWidth, newHeight);

    // Saves new state.
    window.appState.isTrackInfoExpanded_ = newValue;
    appUtil.saveAppState();
  }
};

/**
 * @private
 */
AudioPlayer.prototype.syncHeightForPlaylist_ = function() {
  let targetInnerHeight;

  if (this.player_.playlistExpanded) {
    // playllist expanded.
    if (!this.lastExpandedInnerHeight_ ||
        this.lastExpandedInnerHeight_ < AudioPlayer.EXPANDED_MODE_MIN_HEIGHT) {
      const expandedListHeight =
          Math.min(this.entries_.length, AudioPlayer.DEFAULT_EXPANDED_ITEMS) *
          AudioPlayer.TRACK_HEIGHT;
      if (this.player_.trackInfoExpanded) {
        targetInnerHeight = AudioPlayer.TOP_PADDING_HEIGHT +
                            AudioPlayer.EXPANDED_ARTWORK_HEIGHT +
                            expandedListHeight +
                            AudioPlayer.CONTROLS_HEIGHT;
      } else {
        targetInnerHeight = AudioPlayer.TOP_PADDING_HEIGHT +
                            AudioPlayer.TRACK_HEIGHT +
                            expandedListHeight +
                            AudioPlayer.CONTROLS_HEIGHT;
      }
      this.lastExpandedInnerHeight_ = targetInnerHeight;
    } else {
      targetInnerHeight = this.lastExpandedInnerHeight_;
    }
  } else {
    // playllist not expanded.
    if (this.player_.trackInfoExpanded) {
      targetInnerHeight = AudioPlayer.TOP_PADDING_HEIGHT +
                          AudioPlayer.EXPANDED_ARTWORK_HEIGHT +
                          AudioPlayer.CONTROLS_HEIGHT;
    } else {
      targetInnerHeight = AudioPlayer.TOP_PADDING_HEIGHT +
                          AudioPlayer.TRACK_HEIGHT +
                          AudioPlayer.CONTROLS_HEIGHT;
    }
  }
  window.resizeTo(window.outerWidth,
                  AudioPlayer.HEADER_HEIGHT + targetInnerHeight);
};

/**
 * Create a TrackInfo object encapsulating the information about one track.
 *
 * @param {FileEntry} entry FileEntry to be retrieved the track info from.
 * @constructor
 */
AudioPlayer.TrackInfo = function(entry) {
  this.url = entry.toURL();
  this.title = this.getDefaultTitle();
  this.artist = this.getDefaultArtist();

  this.artworkUrl = '';
  this.active = false;
};

/**
 * @return {string} Default track title (file name extracted from the url).
 */
AudioPlayer.TrackInfo.prototype.getDefaultTitle = function() {
  let title = this.url.split('/').pop();
  const dotIndex = title.lastIndexOf('.');
  if (dotIndex >= 0) {
    title = title.substr(0, dotIndex);
  }
  title = decodeURIComponent(title);
  return title;
};

/**
 * TODO(kaznacheev): Localize.
 */
AudioPlayer.TrackInfo.DEFAULT_ARTIST = 'Unknown Artist';

/**
 * @return {string} 'Unknown artist' string.
 */
AudioPlayer.TrackInfo.prototype.getDefaultArtist = function() {
  return AudioPlayer.TrackInfo.DEFAULT_ARTIST;
};

/**
 * @param {Object} metadata The metadata object.
 * @param {string} error Error string.
 */
AudioPlayer.TrackInfo.prototype.setMetadata = function(
    metadata, error) {
  // TODO(yoshiki): Handle error in better way.
  this.title = metadata.mediaTitle || this.getDefaultTitle();
  this.artist = error || metadata.mediaArtist || this.getDefaultArtist();
  this.artworkUrl = metadata.contentThumbnailUrl || '';
};

AudioPlayer.load();
