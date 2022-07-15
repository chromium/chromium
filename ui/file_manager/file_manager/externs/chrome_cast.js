// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @namespace
 */
chrome.cast = {};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.AutoJoinPolicy
 */
chrome.cast.AutoJoinPolicy = {
  TAB_AND_ORIGIN_SCOPED: 'tab_and_origin_scoped',
  ORIGIN_SCOPED: 'origin_scoped',
  PAGE_SCOPED: 'page_scoped',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.DefaultActionPolicy
 */
chrome.cast.DefaultActionPolicy = {
  CREATE_SESSION: 'create_session',
  CAST_THIS_TAB: 'cast_this_tab',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.Capability
 */
chrome.cast.Capability = {
  VIDEO_OUT: 'video_out',
  AUDIO_OUT: 'audio_out',
  VIDEO_IN: 'video_in',
  AUDIO_IN: 'audio_in',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.ErrorCode
 */
chrome.cast.ErrorCode = {
  CANCEL: 'cancel',
  TIMEOUT: 'timeout',
  API_NOT_INITIALIZED: 'api_not_initialized',
  INVALID_PARAMETER: 'invalid_parameter',
  EXTENSION_NOT_COMPATIBLE: 'extension_not_compatible',
  EXTENSION_MISSING: 'extension_missing',
  RECEIVER_UNAVAILABLE: 'receiver_unavailable',
  SESSION_ERROR: 'session_error',
  CHANNEL_ERROR: 'channel_error',
  LOAD_MEDIA_FAILED: 'load_media_failed',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.ReceiverAvailability
 */
chrome.cast.ReceiverAvailability = {
  AVAILABLE: 'available',
  UNAVAILABLE: 'unavailable',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.SenderPlatform
 */
chrome.cast.SenderPlatform = {
  CHROME: 'chrome',
  IOS: 'ios',
  ANDROID: 'android',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.ReceiverType
 */
chrome.cast.ReceiverType = {
  CAST: 'cast',
  HANGOUT: 'hangout',
  CUSTOM: 'custom',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.ReceiverAction
 */
chrome.cast.ReceiverAction = {
  CAST: 'cast',
  STOP: 'stop',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.SessionStatus
 */
chrome.cast.SessionStatus = {
  CONNECTED: 'connected',
  DISCONNECTED: 'disconnected',
  STOPPED: 'stopped',
};


/**
 * @namespace
 */
chrome.cast.media = {};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.MediaCommand
 */
chrome.cast.media.MediaCommand = {
  PAUSE: 'pause',
  SEEK: 'seek',
  STREAM_VOLUME: 'stream_volume',
  STREAM_MUTE: 'stream_mute',
};


/**
 * @enum {number}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.MetadataType
 */
chrome.cast.media.MetadataType = {
  GENERIC: 0,
  TV_SHOW: 1,
  MOVIE: 2,
  MUSIC_TRACK: 3,
  PHOTO: 4,
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.PlayerState
 */
chrome.cast.media.PlayerState = {
  IDLE: 'IDLE',
  PLAYING: 'PLAYING',
  PAUSED: 'PAUSED',
  BUFFERING: 'BUFFERING',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.ResumeState
 */
chrome.cast.media.ResumeState = {
  PLAYBACK_START: 'PLAYBACK_START',
  PLAYBACK_PAUSE: 'PLAYBACK_PAUSE',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.StreamType
 */
chrome.cast.media.StreamType = {
  BUFFERED: 'BUFFERED',
  LIVE: 'LIVE',
  OTHER: 'OTHER',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.IdleReason
 */
chrome.cast.media.IdleReason = {
  CANCELLED: 'CANCELLED',
  INTERRUPTED: 'INTERRUPTED',
  FINISHED: 'FINISHED',
  ERROR: 'ERROR',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.TrackType
 */
chrome.cast.media.TrackType = {
  TEXT: 'TEXT',
  AUDIO: 'AUDIO',
  VIDEO: 'VIDEO',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.TextTrackType
 */
chrome.cast.media.TextTrackType = {
  SUBTITLES: 'SUBTITLES',
  CAPTIONS: 'CAPTIONS',
  DESCRIPTIONS: 'DESCRIPTIONS',
  CHAPTERS: 'CHAPTERS',
  METADATA: 'METADATA',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.TextTrackEdgeType
 */
chrome.cast.media.TextTrackEdgeType = {
  NONE: 'NONE',
  OUTLINE: 'OUTLINE',
  DROP_SHADOW: 'DROP_SHADOW',
  RAISED: 'RAISED',
  DEPRESSED: 'DEPRESSED',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.TextTrackWindowType
 */
chrome.cast.media.TextTrackWindowType = {
  NONE: 'NONE',
  NORMAL: 'NORMAL',
  ROUNDED_CORNERS: 'ROUNDED_CORNERS',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.TextTrackFontGenericFamily
 */
chrome.cast.media.TextTrackFontGenericFamily = {
  SANS_SERIF: 'SANS_SERIF',
  MONOSPACED_SANS_SERIF: 'MONOSPACED_SANS_SERIF',
  SERIF: 'SERIF',
  MONOSPACED_SERIF: 'MONOSPACED_SERIF',
  CASUAL: 'CASUAL',
  CURSIVE: 'CURSIVE',
  SMALL_CAPITALS: 'SMALL_CAPITALS',
};


/**
 * @enum {string}
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.TextTrackFontStyle
 */
chrome.cast.media.TextTrackFontStyle = {
  NORMAL: 'NORMAL',
  BOLD: 'BOLD',
  BOLD_ITALIC: 'BOLD_ITALIC',
  ITALIC: 'ITALIC',
};


/**
 * @param {!chrome.cast.ErrorCode} code
 * @param {string=} opt_description
 * @param {Object=} opt_details
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.Error
 */
chrome.cast.Error = function(code, opt_description, opt_details) {};

/** @type {!chrome.cast.ErrorCode} */
chrome.cast.Error.prototype.code;

/** @type {?string} */
chrome.cast.Error.prototype.description;

/** @type {Object} */
chrome.cast.Error.prototype.details;


/**
 * @param {!chrome.cast.SenderPlatform} platform
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.SenderApplication
 */
chrome.cast.SenderApplication = function(platform) {};

/** @type {!chrome.cast.SenderPlatform} */
chrome.cast.SenderApplication.prototype.platform;

/** @type {?string} */
chrome.cast.SenderApplication.prototype.url;

/** @type {?string} */
chrome.cast.SenderApplication.prototype.packageId;


/**
 * @param {string} url
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.Image
 */
chrome.cast.Image = function(url) {};

/** @type {string} */
chrome.cast.Image.prototype.url;

/** @type {?number} */
chrome.cast.Image.prototype.height;

/** @type {?number} */
chrome.cast.Image.prototype.width;


/**
 * @param {?number=} opt_level
 * @param {?boolean=} opt_muted
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.Volume
 */
chrome.cast.Volume = function(opt_level, opt_muted) {};

/** @type {?number} */
chrome.cast.Volume.prototype.level;

/** @type {?boolean} */
chrome.cast.Volume.prototype.muted;


/**
 * @param {!chrome.cast.SessionRequest} sessionRequest
 * @param {function(!chrome.cast.Session)} sessionListener
 * @param {function(!chrome.cast.ReceiverAvailability,Array<Object>)}
 *     receiverListener
 * @param {chrome.cast.AutoJoinPolicy=} opt_autoJoinPolicy
 * @param {chrome.cast.DefaultActionPolicy=} opt_defaultActionPolicy
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.ApiConfig
 */
chrome.cast.ApiConfig = function(
    sessionRequest, sessionListener, receiverListener, opt_autoJoinPolicy,
    opt_defaultActionPolicy) {};

/** @type {!chrome.cast.SessionRequest} */
chrome.cast.ApiConfig.prototype.sessionRequest;

/** @type {function(!chrome.cast.Session)} */
chrome.cast.ApiConfig.prototype.sessionListener;

/** @type {function(!chrome.cast.ReceiverAvailability)} */
chrome.cast.ApiConfig.prototype.receiverListener;

/** @type {!chrome.cast.AutoJoinPolicy} */
chrome.cast.ApiConfig.prototype.autoJoinPolicy;

/** @type {!chrome.cast.DefaultActionPolicy} */
chrome.cast.ApiConfig.prototype.defaultActionPolicy;


/**
 * @param {string} appId
 * @param {!Array<chrome.cast.Capability>=} opt_capabilities
 * @param {number=} opt_timeout
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.SessionRequest
 */
chrome.cast.SessionRequest = function(appId, opt_capabilities, opt_timeout) {};

/** @type {string} */
chrome.cast.SessionRequest.prototype.appId;

/** @type {!Array<chrome.cast.Capability>} */
chrome.cast.SessionRequest.prototype.capabilities;

/** @type {number} */
chrome.cast.SessionRequest.prototype.requestSessionTimeout;

/** @type {?string} */
chrome.cast.SessionRequest.prototype.language;


/**
 * @param {string} label
 * @param {string} friendlyName
 * @param {Array<chrome.cast.Capability>=} opt_capabilities
 * @param {chrome.cast.Volume=} opt_volume
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.Receiver
 */
chrome.cast.Receiver = function(
    label, friendlyName, opt_capabilities, opt_volume) {};

/** @type {string} */
chrome.cast.Receiver.prototype.label;

/** @type {string} */
chrome.cast.Receiver.prototype.friendlyName;

/** @type {!Array<!chrome.cast.Capability>} */
chrome.cast.Receiver.prototype.capabilities;

/** @type {chrome.cast.Volume} */
chrome.cast.Receiver.prototype.volume;

/** @type {!chrome.cast.ReceiverType} */
chrome.cast.Receiver.prototype.receiverType;

/** @type {chrome.cast.ReceiverDisplayStatus} */
chrome.cast.Receiver.prototype.displayStatus;


/**
 * @param {string} statusText
 * @param {!Array<chrome.cast.Image>} appImages
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.ReceiverDisplayStatus
 */
chrome.cast.ReceiverDisplayStatus = function(statusText, appImages) {};

/** @type {string} */
chrome.cast.ReceiverDisplayStatus.prototype.statusText;

/** @type {!Array<chrome.cast.Image>} */
chrome.cast.ReceiverDisplayStatus.prototype.appImages;


/**
 * @param {string} sessionId
 * @param {string} appId
 * @param {string} displayName
 * @param {!Array<chrome.cast.Image>} appImages
 * @param {!chrome.cast.Receiver} receiver
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.Session
 */
chrome.cast.Session = function(
    sessionId, appId, displayName, appImages, receiver) {};

/** @type {string} */
chrome.cast.Session.prototype.sessionId;

/** @type {string} */
chrome.cast.Session.prototype.appId;

/** @type {string} */
chrome.cast.Session.prototype.displayName;

/** @type {?string} */
chrome.cast.Session.prototype.statusText;

/** @type {!Array<chrome.cast.Image>} */
chrome.cast.Session.prototype.appImages;

/** @type {!chrome.cast.Receiver} */
chrome.cast.Session.prototype.receiver;

/** @type {!Array<!chrome.cast.SenderApplication>} The applications. */
chrome.cast.Session.prototype.senderApps;

/** @type {!Array<!{name: string}>} The namespaces. */
chrome.cast.Session.prototype.namespaces;

/** @type {!Array<!chrome.cast.media.Media>} */
chrome.cast.Session.prototype.media;

/** @type {!chrome.cast.SessionStatus} */
chrome.cast.Session.prototype.status;

/**
 * @param {number} newLevel
 * @param {function()} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 */
chrome.cast.Session.prototype.setReceiverVolumeLevel = function(
    newLevel, successCallback, errorCallback) {};

/**
 * @param {boolean} muted
 * @param {function()} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 */
chrome.cast.Session.prototype.setReceiverMuted = function(
    muted, successCallback, errorCallback) {};

/**
 * @param {function()} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 */
chrome.cast.Session.prototype.leave = function(
    successCallback, errorCallback) {};

/**
 * @param {function()} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 */
chrome.cast.Session.prototype.stop = function(
    successCallback, errorCallback) {};

/**
 * @param {string} namespace
 * @param {!Object|string} message
 * @param {!function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.Session.prototype.sendMessage = function(
    namespace, message, successCallback, errorCallback) {};

/**
 * @param {function(boolean)} listener
 */
chrome.cast.Session.prototype.addUpdateListener = function(listener) {};

/**
 * @param {function(boolean)} listener
 */
chrome.cast.Session.prototype.removeUpdateListener = function(listener) {};

/**
 * @param {string} namespace
 * @param {function(string,string)} listener
 */
chrome.cast.Session.prototype.addMessageListener = function(
    namespace, listener) {};

/**
 * @param {string} namespace
 * @param {function(string,string)} listener
 */
chrome.cast.Session.prototype.removeMessageListener = function(
    namespace, listener) {};

/**
 * @param {function(!chrome.cast.media.Media)} listener
 */
chrome.cast.Session.prototype.addMediaListener = function(listener) {};

/**
 * @param {function(chrome.cast.media.Media)} listener
 */
chrome.cast.Session.prototype.removeMediaListener = function(listener) {};

/**
 * @param {!chrome.cast.media.LoadRequest} loadRequest
 * @param {function(!chrome.cast.media.Media)} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.Session.prototype.loadMedia = function(
    loadRequest, successCallback, errorCallback) {};


/**
 * @namespace
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast#.timeout
 */
chrome.cast.timeout = {};


/**
 * @const {!Array<number>}
 * @see https://developers.google.com/cast/docs/reference/chrome/
 */
chrome.cast.VERSION;


/**
 * @typedef {!function(?chrome.cast.Receiver, !chrome.cast.ReceiverAction)}
 */
chrome.cast.ReceiverActionListener;


/**
 * @type {boolean}
 */
chrome.cast.isAvailable;


/**
 * @type {boolean}
 */
chrome.cast.usingPresentationApi;


/**
 * @param {!chrome.cast.ApiConfig} apiConfig
 * @param {function()} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 */
chrome.cast.initialize = function(apiConfig, successCallback, errorCallback) {};


/**
 * @param {function(!chrome.cast.Session)} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 * @param {chrome.cast.SessionRequest=} opt_sessionRequest
 * @param {string=} opt_label
 */
chrome.cast.requestSession = function(
    successCallback, errorCallback, opt_sessionRequest, opt_label) {};


/**
 * @param {string} sessionId The id of the session to join.
 */
chrome.cast.requestSessionById = function(sessionId) {};


/**
 * @param {chrome.cast.ReceiverActionListener} listener
 */
chrome.cast.addReceiverActionListener = function(listener) {};


/**
 * @param {chrome.cast.ReceiverActionListener} listener
 */
chrome.cast.removeReceiverActionListener = function(listener) {};


/**
 * @param {string} message The message to log.
 */
chrome.cast.logMessage = function(message) {};


/**
 * @param {!Array<chrome.cast.Receiver>} receivers
 * @param {function()} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 */
chrome.cast.setCustomReceivers = function(
    receivers, successCallback, errorCallback) {};


/**
 * @param {!chrome.cast.Receiver} receiver
 * @param {function()} successCallback
 * @param {function(chrome.cast.Error)} errorCallback
 */
chrome.cast.setReceiverDisplayStatus = function(
    receiver, successCallback, errorCallback) {};


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.GetStatusRequest
 */
chrome.cast.media.GetStatusRequest = function() {};

/** @type {Object} */
chrome.cast.media.GetStatusRequest.prototype.customData;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.PauseRequest
 */
chrome.cast.media.PauseRequest = function() {};

/** @type {Object} */
chrome.cast.media.PauseRequest.prototype.customData;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.PlayRequest
 */
chrome.cast.media.PlayRequest = function() {};

/** @type {Object} */
chrome.cast.media.PlayRequest.prototype.customData;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.SeekRequest
 */
chrome.cast.media.SeekRequest = function() {};

/** @type {?number} */
chrome.cast.media.SeekRequest.prototype.currentTime;

/** @type {?chrome.cast.media.ResumeState} */
chrome.cast.media.SeekRequest.prototype.resumeState;

/** @type {Object} */
chrome.cast.media.SeekRequest.prototype.customData;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.StopRequest
 */
chrome.cast.media.StopRequest = function() {};

/** @type {Object} */
chrome.cast.media.StopRequest.prototype.customData;


/**
 * @param {!chrome.cast.Volume} volume
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.VolumeRequest
 */
chrome.cast.media.VolumeRequest = function(volume) {};

/** @type {!chrome.cast.Volume} */
chrome.cast.media.VolumeRequest.prototype.volume = volume;

/** @type {Object} */
chrome.cast.media.VolumeRequest.prototype.customData;


/**
 * @param {!chrome.cast.media.MediaInfo} mediaInfo
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.LoadRequest
 */
chrome.cast.media.LoadRequest = function(mediaInfo) {};

/** @type {Array<number>} */
chrome.cast.media.LoadRequest.prototype.activeTrackIds;

/** @type {boolean} */
chrome.cast.media.LoadRequest.prototype.autoplay;

/** @type {?number} */
chrome.cast.media.LoadRequest.prototype.currentTime;

/** @type {Object} */
chrome.cast.media.LoadRequest.prototype.customData;

/** @type {!chrome.cast.media.MediaInfo} */
chrome.cast.media.LoadRequest.prototype.media;


/**
 * @param {Array<number>=} opt_activeTrackIds
 * @param {chrome.cast.media.TextTrackStyle=} opt_textTrackStyle
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.EditTracksInfoRequest
 */
chrome.cast.media.EditTracksInfoRequest = function(
    opt_activeTrackIds, opt_textTrackStyle) {};

/** @type {Array<number>} */
chrome.cast.media.EditTracksInfoRequest.prototype.activeTrackIds;

/** @type {?chrome.cast.media.TextTrackStyle} */
chrome.cast.media.EditTracksInfoRequest.prototype.textTrackStyle;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.GenericMediaMetadata
 */
chrome.cast.media.GenericMediaMetadata = function() {};

/** @type {chrome.cast.media.MetadataType} */
chrome.cast.media.GenericMediaMetadata.prototype.metadataType;

/** @type {?string} */
chrome.cast.media.GenericMediaMetadata.prototype.title;

/** @type {?string} */
chrome.cast.media.GenericMediaMetadata.prototype.subtitle;

/** @type {Array<chrome.cast.Image>} */
chrome.cast.media.GenericMediaMetadata.prototype.images;

/** @type {?string} */
chrome.cast.media.GenericMediaMetadata.prototype.releaseDate;

/**
 * @type {chrome.cast.media.MetadataType}
 * @deprecated Please use metadataType instead.
 */
chrome.cast.media.GenericMediaMetadata.prototype.type;

/**
 * @type {?number}
 * @deprecated Use releaseDate instead.
 */
chrome.cast.media.GenericMediaMetadata.prototype.releaseYear;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.MovieMediaMetadata
 */
chrome.cast.media.MovieMediaMetadata = function() {};

/** @type {chrome.cast.media.MetadataType} */
chrome.cast.media.MovieMediaMetadata.prototype.metadataType;

/** @type {?string} */
chrome.cast.media.MovieMediaMetadata.prototype.title;

/** @type {?string} */
chrome.cast.media.MovieMediaMetadata.prototype.studio;

/** @type {?string} */
chrome.cast.media.MovieMediaMetadata.prototype.subtitle;

/** @type {Array<chrome.cast.Image>} */
chrome.cast.media.MovieMediaMetadata.prototype.images;

/** @type {?string} */
chrome.cast.media.MovieMediaMetadata.prototype.releaseDate;

/**
 * @type {chrome.cast.media.MetadataType}
 * @deprecated Please use metadataType instead.
 */
chrome.cast.media.MovieMediaMetadata.prototype.type;

/**
 * @type {?number}
 * @deprecated Use releaseDate instead.
 */
chrome.cast.media.MovieMediaMetadata.prototype.releaseYear;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.TvShowMediaMetadata
 */
chrome.cast.media.TvShowMediaMetadata = function() {};

/** @type {chrome.cast.media.MetadataType} */
chrome.cast.media.TvShowMediaMetadata.prototype.metadataType;

/** @type {?string} */
chrome.cast.media.TvShowMediaMetadata.prototype.seriesTitle;

/** @type {?string} */
chrome.cast.media.TvShowMediaMetadata.prototype.title;

/** @type {?number} */
chrome.cast.media.TvShowMediaMetadata.prototype.season;

/** @type {?number} */
chrome.cast.media.TvShowMediaMetadata.prototype.episode;

/** @type {Array<chrome.cast.Image>} */
chrome.cast.media.TvShowMediaMetadata.prototype.images;

/** @type {?string} */
chrome.cast.media.TvShowMediaMetadata.prototype.originalAirdate;

/**
 * @type {chrome.cast.media.MetadataType}
 * @deprecated Please use metadataType instead.
 */
chrome.cast.media.TvShowMediaMetadata.prototype.type;

/**
 * @type {?string}
 * @deprecated Use title instead.
 */
chrome.cast.media.TvShowMediaMetadata.prototype.episodeTitle;

/**
 * @type {?number}
 * @deprecated Use season instead.
 */
chrome.cast.media.TvShowMediaMetadata.prototype.seasonNumber;

/**
 * @type {?number}
 * @deprecated Use episode instead.
 */
chrome.cast.media.TvShowMediaMetadata.prototype.episodeNumber;

/**
 * @type {?number}
 * @deprecated Use originalAirdate instead.
 */
chrome.cast.media.TvShowMediaMetadata.prototype.releaseYear;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.MusicTrackMediaMetadata
 */
chrome.cast.media.MusicTrackMediaMetadata = function() {};


/** @type {chrome.cast.media.MetadataType} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.metadataType;

/** @type {?string} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.albumName;

/** @type {?string} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.title;

/** @type {?string} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.albumArtist;

/** @type {?string} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.artist;

/** @type {?string} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.composer;

/** @type {?string} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.songName;

/** @type {?number} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.trackNumber;

/** @type {?number} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.discNumber;

/** @type {Array<chrome.cast.Image>} */
chrome.cast.media.MusicTrackMediaMetadata.prototype.images;

/**
 * @type {chrome.cast.media.MetadataType}
 * @deprecated Please use metadataType instead.
 */
chrome.cast.media.MusicTrackMediaMetadata.prototype.type;

/**
 * @type {?string}
 * @deprecated Use artist instead.
 */
chrome.cast.media.MusicTrackMediaMetadata.prototype.artistName;

/**
 * @type {?number}
 * @deprecated Use releaseDate instead.
 */
chrome.cast.media.MusicTrackMediaMetadata.prototype.releaseYear;

/**
 * @type {?string}
 */
chrome.cast.media.MusicTrackMediaMetadata.prototype.releaseDate;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.PhotoMediaMetadata
 */
chrome.cast.media.PhotoMediaMetadata = function() {};

/** @type {chrome.cast.media.MetadataType} */
chrome.cast.media.PhotoMediaMetadata.prototype.metadataType;

/** @type {?string} */
chrome.cast.media.PhotoMediaMetadata.prototype.title;

/** @type {?string} */
chrome.cast.media.PhotoMediaMetadata.prototype.artist;

/** @type {?string} */
chrome.cast.media.PhotoMediaMetadata.prototype.location;

/** @type {Array<chrome.cast.Image>} */
chrome.cast.media.PhotoMediaMetadata.prototype.images;

/** @type {?number} */
chrome.cast.media.PhotoMediaMetadata.prototype.latitude;

/** @type {?number} */
chrome.cast.media.PhotoMediaMetadata.prototype.longitude;

/** @type {?number} */
chrome.cast.media.PhotoMediaMetadata.prototype.width;

/** @type {?number} */
chrome.cast.media.PhotoMediaMetadata.prototype.height;

/** @type {?string} */
chrome.cast.media.PhotoMediaMetadata.prototype.creationDateTime;

/**
 * @type {chrome.cast.media.MetadataType}
 * @deprecated Please use metadataType instead.
 */
chrome.cast.media.PhotoMediaMetadata.prototype.type;


/**
 * @param {string} contentId
 * @param {string} contentType
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.MediaInfo
 */
chrome.cast.media.MediaInfo = function(contentId, contentType) {};

/** @type {string} */
chrome.cast.media.MediaInfo.prototype.contentId;

/** @type {chrome.cast.media.StreamType} */
chrome.cast.media.MediaInfo.prototype.streamType;

/** @type {string} */
chrome.cast.media.MediaInfo.prototype.contentType;

/** @type {*} */
chrome.cast.media.MediaInfo.prototype.metadata;

/** @type {?number} */
chrome.cast.media.MediaInfo.prototype.duration;

/** @type {Array<!chrome.cast.media.Track>} */
chrome.cast.media.MediaInfo.prototype.tracks;

/** @type {?chrome.cast.media.TextTrackStyle} */
chrome.cast.media.MediaInfo.prototype.textTrackStyle;

/** @type {Object} */
chrome.cast.media.MediaInfo.prototype.customData;


/**
 * @param {string} sessionId
 * @param {number} mediaSessionId
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.Media
 */
chrome.cast.media.Media = function(sessionId, mediaSessionId) {};

/** @type {string} */
chrome.cast.media.Media.prototype.sessionId;

/** @type {number} */
chrome.cast.media.Media.prototype.mediaSessionId;

/** @type {chrome.cast.media.MediaInfo} */
chrome.cast.media.Media.prototype.media;

/** @type {number} */
chrome.cast.media.Media.prototype.playbackRate;

/** @type {!chrome.cast.media.PlayerState} */
chrome.cast.media.Media.prototype.playerState;

/** @type {!Array<!chrome.cast.media.MediaCommand>} */
chrome.cast.media.Media.prototype.supportedMediaCommands;

/** @type {!chrome.cast.Volume} */
chrome.cast.media.Media.prototype.volume;

/** @type {?chrome.cast.media.IdleReason} */
chrome.cast.media.Media.prototype.idleReason;

/** @type {Array<number>} */
chrome.cast.media.Media.prototype.activeTrackIds;

/** @type {Object} */
chrome.cast.media.Media.prototype.customData;

/**
 * @type {number}
 * @deprecated Use getEstimatedTime instead.
 */
chrome.cast.media.Media.prototype.currentTime;

/**
 * @param {chrome.cast.media.GetStatusRequest} getStatusRequest
 * @param {function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.media.Media.prototype.getStatus = function(
    getStatusRequest, successCallback, errorCallback) {};

/**
 * @param {chrome.cast.media.PlayRequest} playRequest
 * @param {function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.media.Media.prototype.play = function(
    playRequest, successCallback, errorCallback) {};

/**
 * @param {chrome.cast.media.PauseRequest} pauseRequest
 * @param {function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.media.Media.prototype.pause = function(
    pauseRequest, successCallback, errorCallback) {};

/**
 * @param {!chrome.cast.media.SeekRequest} seekRequest
 * @param {function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.media.Media.prototype.seek = function(
    seekRequest, successCallback, errorCallback) {};

/**
 * @param {chrome.cast.media.StopRequest} stopRequest
 * @param {function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.media.Media.prototype.stop = function(
    stopRequest, successCallback, errorCallback) {};

/**
 * @param {!chrome.cast.media.VolumeRequest} volumeRequest
 * @param {function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.media.Media.prototype.setVolume = function(
    volumeRequest, successCallback, errorCallback) {};

/**
 * @param {!chrome.cast.media.EditTracksInfoRequest} editTracksInfoRequest
 * @param {function()} successCallback
 * @param {function(!chrome.cast.Error)} errorCallback
 */
chrome.cast.media.Media.prototype.editTracksInfo = function(
    editTracksInfoRequest, successCallback, errorCallback) {};

/**
 * @param {!chrome.cast.media.MediaCommand} command
 * @return {boolean}
 */
chrome.cast.media.Media.prototype.supportsCommand = function(command) {};

/**
 * @return {number}
 * @suppress {deprecated} Uses currentTime member to compute estimated time.
 */
chrome.cast.media.Media.prototype.getEstimatedTime = function() {};

/**
 * @param {function(boolean)} listener
 */
chrome.cast.media.Media.prototype.addUpdateListener = function(listener) {};

/**
 * @param {function(boolean)} listener
 */
chrome.cast.media.Media.prototype.removeUpdateListener = function(listener) {};


/**
 * @namespace
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media#.timeout
 */
chrome.cast.media.timeout = {};

/** @type {number} */
chrome.cast.media.timeout.load;

/** @type {number} */
chrome.cast.media.timeout.getStatus;

/** @type {number} */
chrome.cast.media.timeout.play;

/** @type {number} */
chrome.cast.media.timeout.pause;

/** @type {number} */
chrome.cast.media.timeout.seek;

/** @type {number} */
chrome.cast.media.timeout.stop;

/** @type {number} */
chrome.cast.media.timeout.setVolume;

/** @type {number} */
chrome.cast.media.timeout.editTracksInfo;


/**
 * @param {number} trackId
 * @param {!chrome.cast.media.TrackType} trackType
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.Track
 */
chrome.cast.media.Track = function(trackId, trackType) {};

/** @type {number} */
chrome.cast.media.Track.prototype.trackId;

/** @type {?string} */
chrome.cast.media.Track.prototype.trackContentId;

/** @type {?string} */
chrome.cast.media.Track.prototype.trackContentType;

/** @type {!chrome.cast.media.TrackType} */
chrome.cast.media.Track.prototype.type;

/** @type {?string} */
chrome.cast.media.Track.prototype.name;

/** @type {?string} */
chrome.cast.media.Track.prototype.language;

/** @type {?chrome.cast.media.TextTrackType} */
chrome.cast.media.Track.prototype.subtype;

/** @type {Object} */
chrome.cast.media.Track.prototype.customData;


/**
 * @constructor
 * @see https://developers.google.com/cast/docs/reference/chrome/chrome.cast.media.TextTrackStyle
 */
chrome.cast.media.TextTrackStyle = function() {};

/** @type {?string} */
chrome.cast.media.TextTrackStyle.prototype.foregroundColor;

/** @type {?string} */
chrome.cast.media.TextTrackStyle.prototype.backgroundColor;

/** @type {?chrome.cast.media.TextTrackEdgeType} */
chrome.cast.media.TextTrackStyle.prototype.edgeType;

/** @type {?string} */
chrome.cast.media.TextTrackStyle.prototype.edgeColor;

/** @type {?chrome.cast.media.TextTrackWindowType} */
chrome.cast.media.TextTrackStyle.prototype.windowType;

/** @type {?string} */
chrome.cast.media.TextTrackStyle.prototype.windowColor;

/** @type {?number} */
chrome.cast.media.TextTrackStyle.prototype.windowRoundedCornerRadius;

/** @type {?number} */
chrome.cast.media.TextTrackStyle.prototype.fontScale;

/** @type {?string} */
chrome.cast.media.TextTrackStyle.prototype.fontFamily;

/** @type {?chrome.cast.media.TextTrackFontGenericFamily} */
chrome.cast.media.TextTrackStyle.prototype.fontGenericFamily;

/** @type {?chrome.cast.media.TextTrackFontStyle} */
chrome.cast.media.TextTrackStyle.prototype.fontStyle;

/** @type {Object} */
chrome.cast.media.TextTrackStyle.prototype.customData;
