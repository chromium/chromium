// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Video player with chrome's native controls.
 */
class NativeControlsVideoPlayer {
  constructor() {
    /**
     * List of open videos.
     * @private {Array<!FileEntry>}
     */
    this.videos_ = null;

    /**
     * Index of current playing video.
     * @private {number}
     */
    this.currentPos_ = 0;

    /**
     * HTML video element that contains the video player.
     * @private {HTMLVideoElement}
     */
    this.videoElement_ = null;
  }

  /**
   * Initializes the video player window. This method must be called after DOM
   * initialization.
   * @param {!Array<!FileEntry>} videos List of videos.
   */
  prepare(videos) {
    this.videos_ = videos;

    // TODO: Move these setting to html and css file when
    // we are confident to remove the feature flag
    this.videoElement_ =
        assertInstanceof(document.createElement('video'), HTMLVideoElement);
    this.videoElement_.autoPictureInPicture = true;
    this.videoElement_.controls = true;
    this.videoElement_.controlsList = 'nodownload';
    this.videoElement_.style.pointerEvents = 'auto';
    getRequiredElement('video-container').appendChild(this.videoElement_);

    // TODO: remove the element in html when remove the feature flag
    getRequiredElement('controls-wrapper').style.display = 'none';
    getRequiredElement('spinner-container').style.display = 'none';
    getRequiredElement('error-wrapper').style.display = 'none';
    getRequiredElement('thumbnail').style.display = 'none';
    getRequiredElement('cast-container').style.display = 'none';

    this.videoElement_.addEventListener('pause', this.onPause_.bind(this));

    // Restore playback position when duration change.
    // Duration change happens when video is loading where the duration
    // change from NaN to the actual video duration.
    this.videoElement_.addEventListener(
        'durationchange', this.restorePlayState_.bind(this));

    // Clear stored playback position
    this.videoElement_.addEventListener(
        'onmediacomplete', this.onMediaComplete_.bind(this));


    this.preparePlayList_();
    this.addKeyControls_();
  }

  /**
   * Attach arrow box for previous/next track to document and set
   * 'multiple' attribute if user opens more than 1 videos.
   *
   * @private
   */
  preparePlayList_() {
    const videoPlayerElement =
        assertInstanceof(getRequiredElement('video-player'), HTMLDivElement);
    if (this.videos_.length > 1) {
      videoPlayerElement.setAttribute('multiple', true);
    } else {
      videoPlayerElement.removeAttribute('multiple');
    }

    const arrowRight = assertInstanceof(
        queryRequiredElement('.arrow-box .arrow.right'), HTMLDivElement);
    arrowRight.addEventListener(
        'click', this.advance_.bind(this, true /* next track */));
    const arrowLeft = assertInstanceof(
        queryRequiredElement('.arrow-box .arrow.left'), HTMLDivElement);
    arrowLeft.addEventListener(
        'click', this.advance_.bind(this, false /* previous track */));
  }

  /**
   * Add keyboard controls to document.
   *
   * @private
   */
  addKeyControls_() {
    document.addEventListener('keydown', (/** !Event */ event) => {
      const keyboardEvent = /** @type {!KeyboardEvent} */ (event);
      const key =
          (keyboardEvent.ctrlKey && keyboardEvent.shiftKey ? 'Ctrl+Shift+' :
                                                             '') +
          keyboardEvent.key;
      switch (key) {
          // Handle debug shortcut keys.
        case 'Ctrl+Shift+I':
          chrome.fileManagerPrivate.openInspector('normal');
          break;
        case 'Ctrl+Shift+J':
          chrome.fileManagerPrivate.openInspector('console');
          break;
        case 'Ctrl+Shift+C':
          chrome.fileManagerPrivate.openInspector('element');
          break;
        case 'Ctrl+Shift+B':
          chrome.fileManagerPrivate.openInspector('background');
          break;

        case 'k':
        case 'MediaPlayPause':
          this.togglePlayState_();
          break;
        case 'MediaTrackNext':
          this.advance_(true /* next track */);
          break;
        case 'MediaTrackPrevious':
          this.advance_(false /* previous track */);
          break;
        case 'l':
          this.skip_(true /* forward */);
          break;
        case 'j':
          this.skip_(false /* backward */);
          break;
        case 'BrowserBack':
          chrome.app.window.current().close();
          break;
        case 'MediaStop':
          // TODO: Define "Stop" behavior.
          break;
      }
    });

    getRequiredElement('video-container')
        .addEventListener('click', (/** !Event */ event) => {
          const mouseEvent = /** @type {!MouseEvent} */ (event);
          // Turn on loop mode when ctrl+click while the video is playing.
          // If the video is paused, ignore ctrl and play the video.
          if (mouseEvent.ctrlKey && !this.videoElement_.paused) {
            this.setLoopedModeWithFeedback_(true);
            mouseEvent.preventDefault();
          }
        });
  }

  /**
   * Skips forward/backward.
   * @param {boolean} forward Whether to skip forward or backward.
   * @private
   */
  skip_(forward) {
    let secondsToSkip = Math.min(
        NativeControlsVideoPlayer.PROGRESS_MAX_SECONDS_TO_SKIP,
        this.videoElement_.duration *
            NativeControlsVideoPlayer.PROGRESS_MAX_RATIO_TO_SKIP);

    if (!forward) {
      secondsToSkip *= -1;
    }

    this.videoElement_.currentTime = Math.max(
        Math.min(
            this.videoElement_.currentTime + secondsToSkip,
            this.videoElement_.duration),
        0);
  }

  /**
   * Toggle play/pause.
   *
   * @private
   */
  togglePlayState_() {
    if (this.videoElement_.paused) {
      this.videoElement_.play();
    } else {
      this.videoElement_.pause();
    }
  }

  /**
   * Set the looped mode with feedback.
   *
   * @param {boolean} on Whether enabled or not.
   * @private
   */
  setLoopedModeWithFeedback_(on) {
    this.videoElement_.loop = on;
    if (on) {
      this.showNotification_('VIDEO_PLAYER_LOOPED_MODE');
    }
  }

  /**
   * Briefly show a text notification at top left corner.
   *
   * @param {string} identifier String identifier.
   * @private
   */
  showNotification_(identifier) {
    getRequiredElement('toast-content').textContent =
        loadTimeData.getString(identifier);
    getRequiredElement('toast').show();
  }

  /**
   * Plays the first video.
   */
  playFirstVideo() {
    this.currentPos_ = 0;
    this.reloadCurrentVideo_(this.onFirstVideoReady_.bind(this));
  }

  /**
   * Called when the first video is ready after starting to load.
   * Make the video player app window the same dimension as the video source
   * Restrict the app window inside the screen.
   *
   * @private
   */
  onFirstVideoReady_() {
    const videoWidth = this.videoElement_.videoWidth;
    const videoHeight = this.videoElement_.videoHeight;

    const aspect = videoWidth / videoHeight;
    let newWidth = videoWidth;
    let newHeight = videoHeight;

    const shrinkX = newWidth / window.screen.availWidth;
    const shrinkY = newHeight / window.screen.availHeight;
    if (shrinkX > 1 || shrinkY > 1) {
      if (shrinkY > shrinkX) {
        newHeight = newHeight / shrinkY;
        newWidth = newHeight * aspect;
      } else {
        newWidth = newWidth / shrinkX;
        newHeight = newWidth / aspect;
      }
    }

    let oldLeft = window.screenX;
    let oldTop = window.screenY;
    const oldWidth = window.innerWidth;
    const oldHeight = window.innerHeight;

    if (!oldWidth && !oldHeight) {
      oldLeft = window.screen.availWidth / 2;
      oldTop = window.screen.availHeight / 2;
    }

    let appWindow = chrome.app.window.current();
    appWindow.innerBounds.width = Math.round(newWidth);
    appWindow.innerBounds.height = Math.round(newHeight);
    appWindow.outerBounds.left =
        Math.max(0, Math.round(oldLeft - (newWidth - oldWidth) / 2));
    appWindow.outerBounds.top =
        Math.max(0, Math.round(oldTop - (newHeight - oldHeight) / 2));
    appWindow.show();

    this.videoElement_.focus();
    this.videoElement_.play();
  }

  /**
   * Advance to next video when the current one ends.
   * Not using 'ended' event because dragging the timeline
   * thumb to the end when seeking will trigger 'ended' event.
   *
   * @private
   */
  onPause_() {
    this.videoElement_.loop = false;
    if (this.videoElement_.ended) {
      this.advance_(true);
    }
  }

  /**
   * Onmediacomplete handler.
   * Clear playback position when media completes.
   * @private
   */
  onMediaComplete_() {
    this.savePosition(false);
  }

  /**
   * Advances to the next (or previous) track.
   *
   * @param {boolean} direction True to the next, false to the previous.
   * @private
   */
  advance_(direction) {
    const newPos = this.currentPos_ + (direction ? 1 : -1);
    if (newPos < 0 || newPos >= this.videos_.length) {
      return;
    }

    this.currentPos_ = newPos;
    this.reloadCurrentVideo_(() => {
      this.videoElement_.play();
    });
  }

  /**
   * Reloads the current video.
   *
   * @param {function()=} opt_callback Completion callback.
   * @private
   */
  reloadCurrentVideo_(opt_callback) {
    const videoPlayerElement =
        assertInstanceof(getRequiredElement('video-player'), HTMLDivElement);
    if (this.currentPos_ == (this.videos_.length - 1)) {
      videoPlayerElement.setAttribute('last-video', true);
    } else {
      videoPlayerElement.removeAttribute('last-video');
    }

    if (this.currentPos_ === 0) {
      videoPlayerElement.setAttribute('first-video', true);
    } else {
      videoPlayerElement.removeAttribute('first-video');
    }

    const currentVideo = this.videos_[this.currentPos_];
    this.loadVideo_(currentVideo, opt_callback);
  }

  /**
   * Loads the video file.
   * @param {!FileEntry} video Entry of the video to be played.
   * @param {function()=} opt_callback Completion callback.
   * @private
   */
  async loadVideo_(video, opt_callback) {
    document.title = video.name;
    const videoUrl = video.toURL();

    if (opt_callback) {
      this.videoElement_.addEventListener(
          'loadedmetadata', opt_callback, {once: true});
    }

    this.videoElement_.src = videoUrl;

    // Clear all tracks.
    while (this.videoElement_.firstChild) {
      this.videoElement_.firstChild.remove();
    }

    const subtitleUrl = await this.searchSubtitle_(videoUrl);
    if (subtitleUrl) {
      const track =
          assertInstanceof(document.createElement('track'), HTMLTrackElement);
      track.src = subtitleUrl;
      track.kind = 'subtitles';
      track.default = true;
      this.videoElement_.appendChild(track);
    }
  }

  /**
   * Saves the playback position to the persistent storage.
   * @param {boolean} saveAsync True if the position must be saved
   *     asynchronously (required when closing app windows).
   */
  savePosition(saveAsync) {
    if (!this.videoElement_ || !this.videoElement_.duration ||
        this.videoElement_.duration <
            NativeControlsVideoPlayer.RESUME_THRESHOLD_SECONDS) {
      return;
    }

    const resumeTime = this.videoElement_.currentTime -
        NativeControlsVideoPlayer.RESUME_REWIND_SECONDS;
    const ratio = resumeTime / this.videoElement_.duration;

    // If we are too close to the beginning or the end,
    // remove the resume position so that next time we start from the beginning.
    const position = (ratio < NativeControlsVideoPlayer.RESUME_MARGIN ||
                      ratio > (1 - NativeControlsVideoPlayer.RESUME_MARGIN)) ?
        null :
        resumeTime;

    if (saveAsync) {
      saveEntryAsync(this.videoElement_.src, position);
    } else {
      appUtil.AppCache.update(this.videoElement_.src, position);
    }
  }

  /**
   * Resumes the playback position saved in the persistent storage.
   * @private
   */
  restorePlayState_() {
    if (this.videoElement_ && this.videoElement_.seekable &&
        this.videoElement_.duration >=
            NativeControlsVideoPlayer.RESUME_THRESHOLD_SECONDS) {
      appUtil.AppCache.getValue(
          this.videoElement_.src, (/** number */ position) => {
            if (position) {
              this.videoElement_.currentTime = position;
            }
          });
    }
  }

  /**
   * Search subtitle file corresponding to a video.
   * @param {string} url a url of a video.
   * @return {Promise} a Promise returns url of subtitle file, or an empty
   *     string.
   */
  async searchSubtitle_(url) {
    const resolveLocalFileSystemWithExtension = (extension) => {
      const subtitleUrl = this.getSubtitleUrl_(url, extension);
      return new Promise(
          window.webkitResolveLocalFileSystemURL.bind(null, subtitleUrl));
    };

    try {
      const subtitle = await resolveLocalFileSystemWithExtension('.vtt');
      return subtitle.toURL();
    } catch (error) {
      // TODO: figure out if there could be any error other than
      // file not found or not accessible. If not, remove this log.
      console.error(error);
      return '';
    }
  }

  /**
   * Get the subtitle url.
   *
   * @private
   * @param {string} srcUrl Source url of the video file.
   * @param {string} extension Extension of the subtitle we are looking for.
   * @return {string} Subtitle url.
   */
  getSubtitleUrl_(srcUrl, extension) {
    return srcUrl.replace(/\.[^\.]+$/, extension);
  }
}

/**
 * Save an entry asynchronously when exiting app.
 *
 * @param {*} key Key of the entry to be saved.
 * @param {*} value Value of the entry to be saved.
 */
function saveEntryAsync(key, value) {
  // Packaged apps cannot save synchronously.
  // Pass the data to the background page.
  if (!window.saveOnExit) {
    window.saveOnExit = [];
  }
  window.saveOnExit.push({key: key, value: value});
}

/**
 * 10 seconds should be skipped when J/L key is pressed.
 * @const {number}
 */
NativeControlsVideoPlayer.PROGRESS_MAX_SECONDS_TO_SKIP = 10;

/**
 * 20% of duration should be skipped when the video is too short to skip 10
 * seconds.
 * @const {number}
 */
NativeControlsVideoPlayer.PROGRESS_MAX_RATIO_TO_SKIP = 0.2;

/**
 * No resume if we are within this margin from the start or the end.
 * @const {number}
 */
NativeControlsVideoPlayer.RESUME_MARGIN = 0.05;  // 5%

/**
 * No resume for videos shorter than this.
 * @const {number}
 */
NativeControlsVideoPlayer.RESUME_THRESHOLD_SECONDS = 5 * 60;  // 5 min.

/**
 * When resuming rewind back this much.
 * @const {number}
 */
NativeControlsVideoPlayer.RESUME_REWIND_SECONDS = 5;
