// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interval for updating media info (in ms).
 * @type {number}
 * @const
 */
var MEDIA_UPDATE_INTERVAL = 250;

/**
 * The namespace for communication between the cast and the player.
 * @type {string}
 * @const
 */
var CAST_MESSAGE_NAMESPACE = 'urn:x-cast:com.google.chromeos.videoplayer';

/**
 * This class is the dummy class which has same interface as VideoElement. This
 * behaves like VideoElement, and is used for making Chromecast player
 * controlled instead of the true Video Element tag.
 */
class CastVideoElement extends cr.EventTarget {
  /**
   * @param {MediaManager} media Media manager with the media to play.
   * @param {chrome.cast.Session} session Session to play a video on.
   */
  constructor(media, session) {
    super();

    this.mediaManager_ = media;
    this.mediaInfo_ = null;

    this.castMedia_ = null;
    this.castSession_ = session;
    this.currentTime_ = null;
    this.src_ = '';
    this.volume_ = 100;
    this.loop_ = false;
    this.currentMediaPlayerState_ = null;
    this.currentMediaCurrentTime_ = null;
    this.currentMediaDuration_ = null;
    this.playInProgress_ = false;
    this.pauseInProgress_ = false;
    this.errorCode_ = 0;

    /**
     * @type {number}
     * @private
     */
    this.updateTimerId_ = 0;

    /**
     * @type {?string}
     * @private
     */
    this.token_ = null;

    this.onMessageBound_ = this.onMessage_.bind(this);
    this.onCastMediaUpdatedBound_ = this.onCastMediaUpdated_.bind(this);
    this.castSession_.addMessageListener(
        CAST_MESSAGE_NAMESPACE, this.onMessageBound_);
  }

  /**
   * Prepares for unloading this objects.
   */
  dispose() {
    this.unloadMedia_();
    this.castSession_.removeMessageListener(
        CAST_MESSAGE_NAMESPACE, this.onMessageBound_);
  }

  /**
   * Returns a parent node. This must always be null.
   * @type {Element}
   */
  get parentNode() {
    return null;
  }

  /**
   * The total time of the video (in sec).
   * @type {?number}
   */
  get duration() {
    return this.currentMediaDuration_;
  }

  /**
   * The current timestamp of the video (in sec).
   * @type {?number}
   */
  get currentTime() {
    if (this.castMedia_) {
      if (this.castMedia_.idleReason ===
          chrome.cast.media.IdleReason.FINISHED) {
        // Returns the duration.
        return this.currentMediaDuration_;
      } else {
        return this.castMedia_.getEstimatedTime();
      }
    } else {
      return null;
    }
  }

  set currentTime(currentTime) {
    var seekRequest = new chrome.cast.media.SeekRequest();
    seekRequest.currentTime = currentTime;
    this.castMedia_.seek(seekRequest,
        function() {},
        this.onCastCommandError_.wrap(this));
  }

  /**
   * If this video is pauses or not.
   * @type {boolean}
   */
  get paused() {
    if (!this.castMedia_) {
      return false;
    }

    return !this.playInProgress_ &&
        (this.pauseInProgress_ ||
         this.castMedia_.playerState === chrome.cast.media.PlayerState.PAUSED);
  }

  /**
   * If this video is ended or not.
   * @type {boolean}
   */
  get ended() {
    if (!this.castMedia_) {
      return true;
    }

    return !this.playInProgress_ &&
           this.castMedia_.idleReason === chrome.cast.media.IdleReason.FINISHED;
  }

  /**
   * TimeRange object that represents the seekable ranges of the media
   * resource.
   * @type {TimeRanges}
   */
  get seekable() {
    return {
      length: 1,
      start: function(index) {
        return 0;
      },
      end: function(index) {
        return this.currentMediaDuration_;
      },
    };
  }

  /**
   * Value of the volume
   * @type {number}
   */
  get volume() {
    return this.castSession_.receiver.volume.muted ?
               0 :
               this.castSession_.receiver.volume.level;
  }

  set volume(volume) {
    var VOLUME_EPS = 0.01;  // Threshold for ignoring a small change.


    if (this.castSession_.receiver.volume.muted) {
      if (volume < VOLUME_EPS) {
        return;
      }

      // Unmute before setting volume.
      this.castSession_.setReceiverMuted(false,
          function() {},
          this.onCastCommandError_.wrap(this));

      this.castSession_.setReceiverVolumeLevel(volume,
          function() {},
          this.onCastCommandError_.wrap(this));
    } else {
      // Ignores < 1% change.
      var diff = this.castSession_.receiver.volume.level - volume;
      if (Math.abs(diff) < VOLUME_EPS) {
        return;
      }

      if (volume < VOLUME_EPS) {
        this.castSession_.setReceiverMuted(true,
            function() {},
            this.onCastCommandError_.wrap(this));
        return;
      }

      this.castSession_.setReceiverVolumeLevel(volume,
          function() {},
          this.onCastCommandError_.wrap(this));
    }
  }

  /**
   * Returns the source of the current video.
   * @type {?string}
   */
  get src() {
    return null;
  }

  set src(value) {
    // Do nothing.
  }

  /**
   * Returns the flag if the video loops at end or not.
   * @type {boolean}
   */
  get loop() {
    return this.loop_;
  }

  set loop(value) {
    this.loop_ = !!value;
  }

  /**
   * Returns the error object if available.
   * @type {?Object}
   */
  get error() {
    if (this.errorCode_ === 0) {
      return null;
    }

    return {code: this.errorCode_};
  }

  /**
   * Plays the video.
   * @param {boolean=} opt_seeking True when seeking. False otherwise.
   */
  play(opt_seeking) {
    if (this.playInProgress_) {
      return;
    }

    var play = function() {
      // If the casted media is already playing and a pause request is not in
      // progress, we can skip this play request.
      if (this.castMedia_.playerState ===
              chrome.cast.media.PlayerState.PLAYING &&
          !this.pauseInProgress_) {
        this.playInProgress_ = false;
        return;
      }

      var playRequest = new chrome.cast.media.PlayRequest();
      playRequest.customData = {seeking: !!opt_seeking};

      this.castMedia_.play(
          playRequest,
          function() {
            this.playInProgress_ = false;
          }.wrap(this),
          function(error) {
            this.playInProgress_ = false;
            this.onCastCommandError_(error);
          }.wrap(this));
    }.wrap(this);

    this.playInProgress_ = true;

    if (!this.castMedia_) {
      this.load(play);
    } else {
      play();
    }
  }

  /**
   * Pauses the video.
   * @param {boolean=} opt_seeking True when seeking. False otherwise.
   */
  pause(opt_seeking) {
    if (!this.castMedia_) {
      return;
    }

    if (this.pauseInProgress_ ||
        this.castMedia_.playerState === chrome.cast.media.PlayerState.PAUSED) {
      return;
    }

    var pauseRequest = new chrome.cast.media.PauseRequest();
    pauseRequest.customData = {seeking: !!opt_seeking};

    this.pauseInProgress_ = true;
    this.castMedia_.pause(
        pauseRequest,
        function() {
          this.pauseInProgress_ = false;
        }.wrap(this),
        function(error) {
          this.pauseInProgress_ = false;
          this.onCastCommandError_(error);
        }.wrap(this));
  }

  /**
   * Loads the video.
   */
  load(opt_callback) {
    var sendTokenPromise = this.mediaManager_.getToken(false).then(
        function(token) {
          this.token_ = token;
          this.sendMessage_({message: 'push-token', token: token});
        }.bind(this));

    // Resets the error code.
    this.errorCode_ = 0;

    Promise.all([
      sendTokenPromise,
      this.mediaManager_.getUrl(),
      this.mediaManager_.getMime(),
      this.mediaManager_.getThumbnail()]).
        then(function(results) {
          var url = results[1];
          var mime = results[2];  // maybe empty
          var thumbnailUrl = results[3];  // maybe empty

          this.mediaInfo_ = new chrome.cast.media.MediaInfo(url, mime);
          this.mediaInfo_.customData = {
            tokenRequired: true,
            thumbnailUrl: thumbnailUrl,
          };

          var request = new chrome.cast.media.LoadRequest(this.mediaInfo_);
          return new Promise(
              this.castSession_.loadMedia.bind(this.castSession_, request)).
              then(function(media) {
                this.onMediaDiscovered_(media);
                if (opt_callback) {
                  opt_callback();
                }
              }.bind(this));
        }.bind(this)).catch(function(error) {
          this.unloadMedia_();
          this.dispatchEvent(new Event('error'));
          console.error('Cast failed.', error.stack || error);
        }.bind(this));
  }

  /**
   * Unloads the video.
   * @private
   */
  unloadMedia_() {
    if (this.castMedia_) {
      this.castMedia_.stop(null,
          function() {},
          function(error) {
            // Ignores session error, since session may already be closed.
            if (error.code !== chrome.cast.ErrorCode.SESSION_ERROR &&
                error.code !== chrome.cast.ErrorCode.INVALID_PARAMETER) {
              this.onCastCommandError_(error);
            }
          }.wrap(this));

      this.castMedia_.removeUpdateListener(this.onCastMediaUpdatedBound_);
      this.castMedia_ = null;
    }

    clearInterval(this.updateTimerId_);
  }

  /**
   * Sends the message to cast.
   * @param {(!Object|string)} message Message to be sent (Must be JSON-able
   *     object).
   * @private
   */
  sendMessage_(message) {
    this.castSession_.sendMessage(CAST_MESSAGE_NAMESPACE, message,
        function() {}, function(error) {});
  }

  /**
   * Invoked when receiving a message from the cast.
   * @param {string} namespace Namespace of the message.
   * @param {string} messageAsJson Content of message as json format.
   * @private
   */
  onMessage_(namespace, messageAsJson) {
    if (namespace !== CAST_MESSAGE_NAMESPACE || !messageAsJson) {
      return;
    }

    var message = JSON.parse(messageAsJson);
    if (message['message'] === 'request-token') {
      if (message['previousToken'] === this.token_) {
        this.mediaManager_.getToken(true).then(function(token) {
          this.token_ = token;
          this.sendMessage_({message: 'push-token', token: token});
          // TODO(yoshiki): Revokes the previous token.
        }.bind(this)).catch(function(error) {
          // Send an empty token as an error.
          this.sendMessage_({message: 'push-token', token: ''});
          // TODO(yoshiki): Revokes the previous token.
          console.error(error.stack || error);
        });
      } else {
        console.error(
            'New token is requested, but the previous token mismatches.');
      }
    } else if (message['message'] === 'playback-error') {
      if (message['detail'] === 'src-not-supported') {
        this.errorCode_ = MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED;
      }
    }
  }

  /**
   * This method is called periodically to update media information while the
   * media is loaded.
   * @private
   */
  onPeriodicalUpdateTimer_() {
    if (!this.castMedia_) {
      return;
    }

    if (this.castMedia_.playerState === chrome.cast.media.PlayerState.PLAYING) {
      this.onCastMediaUpdated_(true);
    }
  }

  /**
   * This method should be called when a media file is loaded.
   * @param {chrome.cast.media.Media} media Media object which was discovered.
   * @private
   */
  onMediaDiscovered_(media) {
    if (this.castMedia_ !== null) {
      this.unloadMedia_();
      console.info('New media is found and the old media is overridden.');
    }

    this.castMedia_ = media;
    this.onCastMediaUpdated_(true);
    // Notify that the metadata of the video is ready.
    this.dispatchEvent(new Event('loadedmetadata'));

    media.addUpdateListener(this.onCastMediaUpdatedBound_);
    this.updateTimerId_ = setInterval(this.onPeriodicalUpdateTimer_.bind(this),
                                      MEDIA_UPDATE_INTERVAL);
  }

  /**
   * This method should be called when a media command to cast is failed.
   * @param {Object} error Object representing the error.
   * @private
   */
  onCastCommandError_(error) {
    this.unloadMedia_();
    this.dispatchEvent(new Event('error'));
    console.error('Error on sending command to cast.', error.stack || error);
  }

  /**
   * This is called when any media data is updated and by the periodical timer
   * is fired.
   *
   * @param {boolean} alive Media availability. False if it's unavailable.
   * @private
   */
  onCastMediaUpdated_(alive) {
    if (!this.castMedia_) {
      return;
    }

    var media = this.castMedia_;
    if (this.loop_ &&
        media.idleReason === chrome.cast.media.IdleReason.FINISHED &&
        !alive) {
      // Resets the previous media silently.
      this.castMedia_ = null;

      // Replay the current media.
      this.currentMediaPlayerState_ = chrome.cast.media.PlayerState.BUFFERING;
      this.currentMediaCurrentTime_ = 0;
      this.dispatchEvent(new Event('play'));
      this.dispatchEvent(new Event('timeupdate'));
      this.play();
      return;
    }

    if (this.currentMediaPlayerState_ !== media.playerState) {
      var oldPlayState = false;
      var oldState = this.currentMediaPlayerState_;
      if (oldState === chrome.cast.media.PlayerState.BUFFERING ||
          oldState === chrome.cast.media.PlayerState.PLAYING) {
        oldPlayState = true;
      }
      var newPlayState = false;
      var newState = media.playerState;
      if (newState === chrome.cast.media.PlayerState.BUFFERING ||
          newState === chrome.cast.media.PlayerState.PLAYING) {
        newPlayState = true;
      }
      if (!oldPlayState && newPlayState) {
        this.dispatchEvent(new Event('play'));
      }
      if (oldPlayState && !newPlayState) {
        this.dispatchEvent(new Event('pause'));
      }

      this.currentMediaPlayerState_ = newState;
    }
    if (this.currentMediaCurrentTime_ !== media.getEstimatedTime()) {
      this.currentMediaCurrentTime_ = media.getEstimatedTime();
      this.dispatchEvent(new Event('timeupdate'));
    }

    if (this.currentMediaDuration_ !== media.media.duration) {
      // Since recordMediumCount which is called inside recordCastedVideoLangth
      // can take a value ranges from 1 to 10,000, we don't allow to pass 0
      // here. i.e. length 0 is not recorded.
      if (this.currentMediaDuration_) {
        metrics.recordCastedVideoLength(this.currentMediaDuration_);
      }

      this.currentMediaDuration_ = media.media.duration;
      this.dispatchEvent(new Event('durationchange'));
    }

    // Media is being unloaded.
    if (!alive) {
      this.unloadMedia_();
      return;
    }
  }
}
