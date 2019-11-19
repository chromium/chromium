// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Boolean flag used to toggle native control
 * @type {boolean}
 */
let useNativeControls = false;

/**
 * @param {!HTMLElement} playerContainer Main container.
 * @param {!HTMLElement} videoContainer Container for the video element.
 * @param {!HTMLElement} controlsContainer Container for video controls.
 * @constructor
 * @struct
 * @extends {VideoControls}
 */
function FullWindowVideoControls(
    playerContainer, videoContainer, controlsContainer) {
  VideoControls.call(this,
      controlsContainer,
      this.onPlaybackError_.wrap(this),
      this.toggleFullScreen_.wrap(this),
      videoContainer);

  this.playerContainer_ = playerContainer;
  this.decodeErrorOccured = false;

  this.casting = false;
  this.isRtl_ =
      window.getComputedStyle(this.playerContainer_)['direction'] === 'rtl';

  var currentWindow = chrome.app.window.current();
  currentWindow.onFullscreened.addListener(this.onFullScreenChanged.bind(this));
  currentWindow.onRestored.addListener(this.onFullScreenChanged.bind(this));
  document.addEventListener('keydown', function(e) {
    this.inactivityWatcher_.kick();
    switch (util.getKeyModifiers(e) + e.key) {
      // Handle debug shortcut keys.
      case 'Ctrl-Shift-I': // Ctrl+Shift+I
        chrome.fileManagerPrivate.openInspector('normal');
        break;
      case 'Ctrl-Shift-J': // Ctrl+Shift+J
        chrome.fileManagerPrivate.openInspector('console');
        break;
      case 'Ctrl-Shift-C': // Ctrl+Shift+C
        chrome.fileManagerPrivate.openInspector('element');
        break;
      case 'Ctrl-Shift-B': // Ctrl+Shift+B
        chrome.fileManagerPrivate.openInspector('background');
        break;

      case ' ': // Space
      case 'k':
      case 'MediaPlayPause':
        if (!e.target.classList.contains('menu-button')) {
          this.togglePlayStateWithFeedback();
        }
        break;
      case 'Escape':
        util.toggleFullScreen(
            chrome.app.window.current(),
            false);  // Leave the full screen mode.
        break;
      case 'MediaTrackNext':
        player.advance_(1);
        break;
      case 'MediaTrackPrevious':
        player.advance_(0);
        break;
      case 'ArrowRight':
        if (!e.target.classList.contains('volume')) {
          this.smallSkip(!this.isRtl_ /* forward */);
        }
        break;
      case 'ArrowLeft':
        if (!e.target.classList.contains('volume')) {
          this.smallSkip(this.isRtl_ /* forward */);
        }
        break;
      case 'l':
        this.bigSkip(true /* forward */);
        break;
      case 'j':
        this.bigSkip(false /* forward */);
        break;
      case 'BrowserBack':
        chrome.app.window.current().close();
        break;
      case 'MediaStop':
        // TODO: Define "Stop" behavior.
        break;
    }
  }.wrap(this));
  document.addEventListener('keypress', function(e) {
    this.inactivityWatcher_.kick();
  }.wrap(this));
  controlsContainer.addEventListener('cr-slider-value-changed', () => {
    this.inactivityWatcher_.kick();
  });

  // TODO(mtomasz): Simplify. crbug.com/254318.
  var clickInProgress = false;
  videoContainer.addEventListener('click', function(e) {
    if (clickInProgress) {
      return;
    }

    clickInProgress = true;
    var togglePlayState = function() {
      clickInProgress = false;

      if (e.ctrlKey) {
        this.toggleLoopedModeWithFeedback(true);
        if (!this.isPlaying()) {
          this.togglePlayStateWithFeedback();
        }
      } else {
        this.togglePlayStateWithFeedback();
      }
    }.wrap(this);

    if (!this.media_) {
      player.reloadCurrentVideo(togglePlayState);
    } else {
      setTimeout(togglePlayState, 0);
    }
  }.wrap(this));

  /**
   * @type {MouseInactivityWatcher}
   * @private
   */
  this.inactivityWatcher_ = new MouseInactivityWatcher(playerContainer);
  this.inactivityWatcher_.check();
}

FullWindowVideoControls.prototype = { __proto__: VideoControls.prototype };

/**
 * Gets inactivity watcher.
 * @return {MouseInactivityWatcher} An inactivity watcher.
 */
FullWindowVideoControls.prototype.getInactivityWatcher = function() {
  return this.inactivityWatcher_;
};

/**
 * Displays error message.
 *
 * @param {string} message Message id.
 */
FullWindowVideoControls.prototype.showErrorMessage = function(message) {
  var errorBanner = getRequiredElement('error');
  errorBanner.textContent = str(message);
  errorBanner.setAttribute('visible', 'true');

  // The window is hidden if the video has not loaded yet.
  chrome.app.window.current().show();
};

/**
 * Handles playback (decoder) errors.
 * @param {MediaError} error Error object.
 * @private
 */
FullWindowVideoControls.prototype.onPlaybackError_ = function(error) {
  if (error.target && error.target.error &&
      error.target.error.code === MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED) {
    if (this.casting) {
      this.showErrorMessage('VIDEO_PLAYER_VIDEO_FILE_UNSUPPORTED_FOR_CAST');
    } else {
      this.showErrorMessage('VIDEO_PLAYER_VIDEO_FILE_UNSUPPORTED');
    }
    this.decodeErrorOccured = false;
  } else {
    this.showErrorMessage('VIDEO_PLAYER_PLAYBACK_ERROR');
    this.decodeErrorOccured = true;
  }

  // Disable controls on the ui.
  getRequiredElement('video-player').setAttribute('disabled', 'true');

  // Detach the video element, since it may be unreliable and reset stored
  // current playback time.
  this.cleanup();
  this.clearState();

  // Avoid reusing a video element.
  player.unloadVideo();
};

/**
 * Toggles the full screen mode.
 * @private
 */
FullWindowVideoControls.prototype.toggleFullScreen_ = function() {
  var appWindow = chrome.app.window.current();
  util.toggleFullScreen(appWindow, !util.isFullScreen(appWindow));
};

/**
 * Media completion handler.
 */
FullWindowVideoControls.prototype.onMediaComplete = function() {
  VideoControls.prototype.onMediaComplete.apply(this, arguments);
  if (!this.getMedia().loop) {
    player.advance_(1);
  }
};

/**
 * Video Player
 *
 * @constructor
 * @struct
 */
function VideoPlayer() {
  this.controls_ = null;
  this.videoElement_ = null;

  /**
   * @type {Array<!FileEntry>}
   * @private
   */
  this.videos_ = null;

  this.currentPos_ = 0;

  this.currentSession_ = null;
  this.currentCast_ = null;

  this.loadQueue_ = new AsyncUtil.Queue();

  this.onCastSessionUpdateBound_ = this.onCastSessionUpdate_.wrap(this);
}

VideoPlayer.prototype = /** @struct */ {
  /**
   * @return {FullWindowVideoControls}
   */
  get controls() {
    return this.controls_;
  }
};

/**
 * Initializes the video player window. This method must be called after DOM
 * initialization.
 * @param {!Array<!FileEntry>} videos List of videos.
 */
VideoPlayer.prototype.prepare = function(videos) {
  this.videos_ = videos;

  var preventDefault = function(event) {
    event.preventDefault();
  }.wrap(null);

  document.ondragstart = preventDefault;

  cr.ui.decorate(getRequiredElement('cast-menu'), cr.ui.Menu);

  this.controls_ = new FullWindowVideoControls(
      getRequiredElement('video-player'),
      getRequiredElement('video-container'),
      getRequiredElement('controls'));

  var observer = new MutationObserver(function(mutations) {
    var isLoadingOrDisabledChanged = mutations.some(function(mutation) {
      return mutation.attributeName === 'loading' ||
             mutation.attributeName === 'disabled';
    });
    if (isLoadingOrDisabledChanged) {
      this.updateInactivityWatcherState_();
    }
  }.bind(this));
  observer.observe(getRequiredElement('video-player'),
      { attributes: true, childList: false });

  var reloadVideo = function(e) {
    if (this.controls_.decodeErrorOccured &&
        // Ignore shortcut keys
        !e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey) {
      this.reloadCurrentVideo(function() {
        this.videoElement_.play();
      }.wrap(this));
      e.preventDefault();
    }
  }.wrap(this);

  var arrowRight = queryRequiredElement('.arrow-box .arrow.right');
  arrowRight.addEventListener('click', this.advance_.wrap(this, 1));
  var arrowLeft = queryRequiredElement('.arrow-box .arrow.left');
  arrowLeft.addEventListener('click', this.advance_.wrap(this, 0));

  var videoPlayerElement = getRequiredElement('video-player');
  if (videos.length > 1) {
    videoPlayerElement.setAttribute('multiple', true);
  } else {
    videoPlayerElement.removeAttribute('multiple');
  }

  var castButton = queryRequiredElement('.cast-button');
  castButton.addEventListener('click',
      this.onCastButtonClicked_.wrap(this));

  document.addEventListener('keydown', reloadVideo);
  document.addEventListener('click', reloadVideo);
};

/**
 * Unloads the player.
 */
function unload() {
  // Releases keep awake just in case (should be released on unloading video).
  chrome.power.releaseKeepAwake();

  if (useNativeControls) {
    nativePlayer.savePosition(true);
    return;
  }

  if (!player.controls || !player.controls.getMedia()) {
    return;
  }

  player.controls.savePosition(true /* exiting */);
  player.controls.cleanup();
}

/**
 * Loads the video file.
 * @param {!FileEntry} video Entry of the video to be played.
 * @param {function()=} opt_callback Completion callback.
 * @private
 */
VideoPlayer.prototype.loadVideo_ = function(video, opt_callback) {
  this.unloadVideo(true);

  this.loadQueue_.run(function(callback) {
    document.title = video.name;

    var videoPlayerElement = getRequiredElement('video-player');
    if (this.currentPos_ === (this.videos_.length - 1)) {
      videoPlayerElement.setAttribute('last-video', true);
    } else {
      videoPlayerElement.removeAttribute('last-video');
    }

    if (this.currentPos_ === 0) {
      videoPlayerElement.setAttribute('first-video', true);
    } else {
      videoPlayerElement.removeAttribute('first-video');
    }

    // Re-enables ui and hides error message if already displayed.
    getRequiredElement('video-player').removeAttribute('disabled');
    getRequiredElement('error').removeAttribute('visible');
    this.controls.detachMedia();
    this.controls.decodeErrorOccured = false;
    this.controls.casting = !!this.currentCast_;

    videoPlayerElement.setAttribute('loading', true);

    var media = new MediaManager(video);

    // Show video's thumbnail if available while loading the video.
    media.getThumbnail()
        .then(function(thumbnailUrl) {
          if (!thumbnailUrl) {
            return Promise.reject();
          }

          return new Promise(function(resolve, reject) {
            ImageLoaderClient.getInstance().load(
                thumbnailUrl, function(result) {
                  if (result.data) {
                    resolve(result.data);
                  } else {
                    reject();
                  }
                });
          });
        })
        .then(function(dataUrl) {
          getRequiredElement('thumbnail').style.backgroundImage =
              'url(' + dataUrl + ')';
        })
        .catch(function() {
          // Shows no image on error.
          getRequiredElement('thumbnail').style.backgroundImage = '';
        });

    var videoElementInitializePromise;
    if (this.currentCast_) {
      metrics.recordPlayType(metrics.PLAY_TYPE.CAST);

      getRequiredElement('cast-name').textContent =
          this.currentCast_.friendlyName;

      videoPlayerElement.setAttribute('castable', true);

      videoElementInitializePromise =
          media.isAvailableForCast().then(function(result) {
            if (!result) {
              return Promise.reject('No casts are available.');
            }

            return new Promise(function(fulfill, reject) {
              if (this.currentSession_) {
                fulfill(this.currentSession_);
              } else {
                chrome.cast.requestSession(
                    fulfill, reject, undefined, this.currentCast_.label);
              }
            }.bind(this)).then(function(session) {
              videoPlayerElement.setAttribute('casting', true);
              session.addUpdateListener(this.onCastSessionUpdateBound_);

              this.currentSession_ = session;
              this.videoElement_ = new CastVideoElement(media, session);
            }.bind(this));
          }.bind(this));
    } else {
      metrics.recordPlayType(metrics.PLAY_TYPE.LOCAL);
      videoPlayerElement.removeAttribute('casting');

      this.videoElement_ = document.createElement('video');
      this.videoElement_.autoPictureInPicture = true;
      getRequiredElement('video-container').appendChild(this.videoElement_);

      var videoUrl = video.toURL();
      var source = document.createElement('source');
      source.src = videoUrl;
      this.videoElement_.appendChild(source);

      media.isAvailableForCast()
          .then(function(result) {
            if (result) {
              videoPlayerElement.setAttribute('castable', true);
            } else {
              videoPlayerElement.removeAttribute('castable');
            }
          })
          .catch(function() {
            videoPlayerElement.setAttribute('castable', true);
          });

      videoElementInitializePromise = this.searchSubtitle_(videoUrl)
          .then(function(subltitleUrl) {
            if (subltitleUrl) {
              var track = document.createElement('track');
              track.src = subltitleUrl;
              track.kind = 'subtitles';
              track.default = true;
              this.videoElement_.appendChild(track);
            }
          }.bind(this));
    }
    videoElementInitializePromise
        .then(function() {
          var handler = function(currentPos) {
            if (currentPos === this.currentPos_) {
              if (opt_callback) {
                opt_callback();
              }
              videoPlayerElement.removeAttribute('loading');
            }

            this.videoElement_.removeEventListener('loadedmetadata', handler);
          }.wrap(this, this.currentPos_);

          this.videoElement_.addEventListener('loadedmetadata', handler);

          this.videoElement_.addEventListener('play', function() {
            chrome.power.requestKeepAwake('display');
            this.updateInactivityWatcherState_();
            this.updateMediaSessionPlaybackState_();
          }.wrap(this));
          this.videoElement_.addEventListener('pause', function() {
            chrome.power.releaseKeepAwake();
            this.updateInactivityWatcherState_();
            this.updateMediaSessionPlaybackState_();
          }.wrap(this));
          this.controls.attachMedia(this.videoElement_);
          this.videoElement_.load();
          callback();
        }.bind(this))
        // In case of error.
        .catch(function(error) {
          if (this.currentCast_) {
            metrics.recordCastVideoErrorAction();
          }

          videoPlayerElement.removeAttribute('loading');
          console.error('Failed to initialize the video element.',
                        error.stack || error);
          this.controls_.showErrorMessage(
              'VIDEO_PLAYER_VIDEO_FILE_UNSUPPORTED');
          callback();
        }.bind(this));
  }.wrap(this));
};

/**
 * Search subtile file corresponding to a video.
 * @param {string} url a url of a video.
 * @return {string} a url of subtitle file, or an empty string.
 */
VideoPlayer.prototype.searchSubtitle_ = function(url) {
  var baseUrl = util.splitExtension(url)[0];
  var resolveLocalFileSystemWithExtension = function(extension) {
    return new Promise(
        window.webkitResolveLocalFileSystemURL.bind(null, baseUrl + extension));
  };
  return resolveLocalFileSystemWithExtension('.vtt').then(function(subtitle) {
    return subtitle.toURL();
  }).catch(function() {
    return '';
  });
};

/**
 * Plays the first video.
 */
VideoPlayer.prototype.playFirstVideo = function() {
  this.currentPos_ = 0;
  this.reloadCurrentVideo(this.onFirstVideoReady_.wrap(this));
};

/**
 * Unloads the current video.
 * @param {boolean=} opt_keepSession If true, keep using the current session.
 *     Otherwise, discards the session.
 */
VideoPlayer.prototype.unloadVideo = function(opt_keepSession) {
  this.loadQueue_.run(function(callback) {
    chrome.power.releaseKeepAwake();

    // Detaches the media from the control.
    this.controls.detachMedia();

    if (this.videoElement_) {
      // If the element has dispose method, call it (CastVideoElement has it).
      if (this.videoElement_.dispose) {
        this.videoElement_.dispose();
      }
      // Detach the previous video element, if exists.
      if (this.videoElement_.parentNode) {
        this.videoElement_.parentNode.removeChild(this.videoElement_);
      }
    }
    this.videoElement_ = null;

    if (!opt_keepSession && this.currentSession_) {
      // We should not request stop() if the current session is not connected to
      // the receiver.
      if (this.currentSession_.status === chrome.cast.SessionStatus.CONNECTED) {
        this.currentSession_.stop(callback, callback);
      } else {
        setTimeout(callback);
      }
      this.currentSession_.removeUpdateListener(this.onCastSessionUpdateBound_);
      this.currentSession_ = null;
    } else {
      callback();
    }
  }.wrap(this));
};

/**
 * Called when the first video is ready after starting to load.
 * @private
 */
VideoPlayer.prototype.onFirstVideoReady_ = function() {
  var videoWidth = this.videoElement_.videoWidth;
  var videoHeight = this.videoElement_.videoHeight;

  var aspect = videoWidth / videoHeight;
  var newWidth = videoWidth;
  var newHeight = videoHeight;

  var shrinkX = newWidth / window.screen.availWidth;
  var shrinkY = newHeight / window.screen.availHeight;
  if (shrinkX > 1 || shrinkY > 1) {
    if (shrinkY > shrinkX) {
      newHeight = newHeight / shrinkY;
      newWidth = newHeight * aspect;
    } else {
      newWidth = newWidth / shrinkX;
      newHeight = newWidth / aspect;
    }
  }

  var oldLeft = window.screenX;
  var oldTop = window.screenY;
  var oldWidth = window.innerWidth;
  var oldHeight = window.innerHeight;

  if (!oldWidth && !oldHeight) {
    oldLeft = window.screen.availWidth / 2;
    oldTop = window.screen.availHeight / 2;
  }

  var appWindow = chrome.app.window.current();
  appWindow.innerBounds.width = Math.round(newWidth);
  appWindow.innerBounds.height = Math.round(newHeight);
  appWindow.outerBounds.left = Math.max(
      0, Math.round(oldLeft - (newWidth - oldWidth) / 2));
  appWindow.outerBounds.top = Math.max(
      0, Math.round(oldTop - (newHeight - oldHeight) / 2));
  appWindow.show();

  this.videoElement_.play();
};

/**
 * Advances to the next (or previous) track.
 *
 * @param {boolean} direction True to the next, false to the previous.
 * @private
 */
VideoPlayer.prototype.advance_ = function(direction) {
  var newPos = this.currentPos_ + (direction ? 1 : -1);
  if (0 <= newPos && newPos < this.videos_.length) {
    this.currentPos_ = newPos;
    this.reloadCurrentVideo(function() {
      this.videoElement_.play();
    }.wrap(this));
  }
};

/**
 * Reloads the current video.
 *
 * @param {function()=} opt_callback Completion callback.
 */
VideoPlayer.prototype.reloadCurrentVideo = function(opt_callback) {
  var currentVideo = this.videos_[this.currentPos_];
  this.loadVideo_(currentVideo, opt_callback);
};

/**
 * Invokes when a menuitem in the cast menu is selected.
 * @param {Object} cast Selected element in the list of casts.
 * @private
 */
VideoPlayer.prototype.onCastSelected_ = function(cast) {
  // If the selected item is same as the current item, do nothing.
  if ((this.currentCast_ && this.currentCast_.label) === (cast && cast.label)) {
    return;
  }

  this.unloadVideo(false);

  // Waits for unloading video.
  this.loadQueue_.run(function(callback) {
    this.currentCast_ = cast || null;
    this.updateCheckOnCastMenu_();
    this.reloadCurrentVideo();
    callback();
  }.wrap(this));
};

/**
 * Set the list of casts.
 * @param {Array<Object>} casts List of casts.
 */
VideoPlayer.prototype.setCastList = function(casts) {
  var videoPlayerElement = getRequiredElement('video-player');
  var menu = getRequiredElement('cast-menu');
  menu.innerHTML = '';

  // TODO(yoshiki): Handle the case that the current cast disappears.

  if (casts.length === 0) {
    videoPlayerElement.removeAttribute('cast-available');
    if (this.currentCast_) {
      this.onCurrentCastDisappear_();
    }
    return;
  }

  if (this.currentCast_) {
    var currentCastAvailable = casts.some(function(cast) {
      return this.currentCast_.label === cast.label;
    }.wrap(this));

    if (!currentCastAvailable) {
      this.onCurrentCastDisappear_();
    }
  }

  var item = new cr.ui.MenuItem();
  item.label = str('VIDEO_PLAYER_PLAY_THIS_COMPUTER');
  item.setAttribute('aria-label', item.label);
  item.castLabel = '';
  item.addEventListener('activate', this.onCastSelected_.wrap(this, null));
  menu.appendChild(item);

  for (var i = 0; i < casts.length; i++) {
    var item = new cr.ui.MenuItem();
    item.label = casts[i].friendlyName;
    item.setAttribute('aria-label', item.label);
    item.castLabel = casts[i].label;
    item.addEventListener('activate',
                          this.onCastSelected_.wrap(this, casts[i]));
    menu.appendChild(item);
  }
  this.updateCheckOnCastMenu_();
  videoPlayerElement.setAttribute('cast-available', true);
};

/**
 * Tells the availability of cast receivers to VideoPlayeru topdate the
 * visibility of the cast button..
 * @param {boolean} available Whether at least one cast receiver is available.
 */
VideoPlayer.prototype.setCastAvailability = function(available) {
  var videoPlayerElement = getRequiredElement('video-player');
  if (available) {
    videoPlayerElement.setAttribute('mr-cast-available', true);
  } else {
    videoPlayerElement.removeAttribute('mr-cast-available');
    if (this.currentCast_) {
      this.onCurrentCastDisappear_();
    }
  }
};

/**
 * Handles click event on cast button to request a session.
 * @private
 */
VideoPlayer.prototype.onCastButtonClicked_ = function() {
  // This method is called only when Media Router is enabled.
  // In this case, requestSession() will open a built-in dialog (not a dropdown
  // menu) to choose the receiver, and callback is called with the session
  // object after user's operation..
  chrome.cast.requestSession(
      function(session) {
        this.unloadVideo(true);
        this.loadQueue_.run(function(callback) {
          this.currentCast_ = {
            label: session.receiver.label,
            friendlyName: session.receiver.friendlyName
          };
          this.currentSession_ = session;
          this.reloadCurrentVideo();
          callback();
        }.bind(this));
      }.bind(this),
      function(error) {
        if (error.code !== chrome.cast.ErrorCode.CANCEL) {
          console.error('requestSession from cast button failed', error);
        }
      });
};

/**
 * Updates the check status of the cast menu items.
 * @private
 */
VideoPlayer.prototype.updateCheckOnCastMenu_ = function() {
  var menuItems =
      /** @type {cr.ui.Menu} */ (getRequiredElement('cast-menu')).menuItems;
  for (var i = 0; i < menuItems.length; i++) {
    var item = menuItems[i];
    if (this.currentCast_ === null) {
      // Playing on this computer.
      if (item.castLabel === '') {
        item.checked = true;
      } else {
        item.checked = false;
      }
    } else {
      // Playing on cast device.
      if (item.castLabel === this.currentCast_.label) {
        item.checked = true;
      } else {
        item.checked = false;
      }
    }
  }
};

/**
 * Called when the current cast is disappear from the cast list.
 * @private
 */
VideoPlayer.prototype.onCurrentCastDisappear_ = function() {
  this.currentCast_ = null;
  if (this.currentSession_) {
    this.currentSession_.removeUpdateListener(this.onCastSessionUpdateBound_);
    this.currentSession_ = null;
  }
  this.controls.showErrorMessage('VIDEO_PLAYER_PLAYBACK_ERROR');
  this.unloadVideo();
};

/**
 * This method should be called when the session is updated.
 * @param {boolean} alive Whether the session is alive or not.
 * @private
 */
VideoPlayer.prototype.onCastSessionUpdate_ = function(alive) {
  if (!alive) {
    var videoPlayerElement = getRequiredElement('video-player');
    videoPlayerElement.removeAttribute('casting');

    // Loads the current video in local player.
    this.unloadVideo();
    this.loadQueue_.run(function(callback) {
      this.currentCast_ = null;
      if (!chrome.cast.usingPresentationApi) {
        this.updateCheckOnCastMenu_();
      }
      this.reloadCurrentVideo();
      callback();
    }.wrap(this));
  }
};

/**
 * Updates the MouseInactivityWatcher's disable property to prevent control
 * panel from being hidden in some situations.
 * @private
 */
VideoPlayer.prototype.updateInactivityWatcherState_ = function() {
  var videoPlayerElement = getRequiredElement('video-player');
  // If any of following condition is met, we don't hide the tool bar.
  // - Loaded video is paused.
  // - Loading a video is in progress.
  // - Opening video has an error.
  this.controls.getInactivityWatcher().disabled =
      (this.videoElement_ && this.videoElement_.paused) ||
      videoPlayerElement.hasAttribute('loading') ||
      videoPlayerElement.hasAttribute('disabled');
};

/**
 * Updates the Media Session API with the current playback state of the video
 * element.
 * @private
 */
VideoPlayer.prototype.updateMediaSessionPlaybackState_ = function() {
  if (!navigator.mediaSession) {
    return;
  }

  navigator.mediaSession.playbackState =
      (this.videoElement_ && !this.videoElement_.paused) ?
      MediaSessionPlaybackState.PLAYING :
      MediaSessionPlaybackState.PAUSED;
};

var player = new VideoPlayer();

let nativePlayer = new NativeControlsVideoPlayer();

/**
 * Initializes the load time data.
 * @param {function()} callback Called when the load time data is ready.
 */
function initStrings(callback) {
  chrome.fileManagerPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;
    useNativeControls =
        loadTimeData.getBoolean('VIDEO_PLAYER_NATIVE_CONTROLS_ENABLED');
    callback();
  }.wrap(null));
}

/**
 * Initializes the volume manager.
 * @param {function()} callback Called when the volume manager is ready.
 */
function initVolumeManager(callback) {
  var volumeManager = new FilteredVolumeManager(AllowedPaths.ANY_PATH, false);
  volumeManager.ensureInitialized(callback);
}

/**
 * Promise to initialize both the volume manager and the load time data.
 * @type {!Promise}
 */
const initPromise = Promise.all([
  new Promise(initStrings.wrap(null)),
  new Promise(initVolumeManager.wrap(null)),
]);

/**
 * Initialize the video player.
 */
initPromise
    .then(function() {
      if (document.readyState !== 'loading') {
        return;
      }
      return new Promise(function(fulfill, reject) {
        document.addEventListener('DOMContentLoaded', fulfill);
      }.wrap());
    }.wrap())
    .then(function() {
      const isReady = document.readyState !== 'loading';
      assert(isReady, 'VideoPlayer DOM document is still loading');
      i18nTemplate.process(document, loadTimeData);
      return new Promise(function(fulfill, reject) {
        util.URLsToEntries(window.appState.items, function(entries) {
          metrics.recordOpenVideoPlayerAction();
          metrics.recordNumberOfOpenedFiles(entries.length);

          if (!useNativeControls) {
            player.prepare(entries);
            player.playFirstVideo(player, fulfill);
          } else {
            nativePlayer.prepare(entries);
            nativePlayer.playFirstVideo();
          }
        }.wrap());
      }.wrap());
    }.wrap());
