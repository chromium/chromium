/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview provides a reusable youtube UI component given a youtube data
 * model.
 *
 * goog.ui.media.Youtube is actually a {@link goog.ui.ControlRenderer}, a
 * stateless class - that could/should be used as a Singleton with the static
 * method `goog.ui.media.Youtube.getInstance` -, that knows how to render
 * youtube videos. It is designed to be used with a {@link goog.ui.Control},
 * which will actually control the media renderer and provide the
 * {@link goog.ui.Component} base. This design guarantees that all different
 * types of medias will behave alike but will look different.
 *
 * goog.ui.media.Youtube expects `goog.ui.media.YoutubeModel` on
 * `goog.ui.Control.getModel` as data models, and render a flash object
 * that will play that URL.
 *
 * Example of usage:
 *
 * <pre>
 *   var video = goog.ui.media.YoutubeModel.newInstance(
 *       'https://www.youtube.com/watch?v=ddl5f44spwQ');
 *   goog.ui.media.Youtube.newControl(video).render();
 * </pre>
 *
 * youtube medias currently support the following states:
 *
 * <ul>
 *   <li> {@link goog.ui.Component.State.DISABLED}: shows 'flash not available'
 *   <li> {@link goog.ui.Component.State.HOVER}: mouse cursor is over the video
 *   <li> {@link !goog.ui.Component.State.SELECTED}: a static thumbnail is shown
 *   <li> {@link goog.ui.Component.State.SELECTED}: video is playing
 * </ul>
 *
 * Which can be accessed by
 * <pre>
 *   youtube.setEnabled(true);
 *   youtube.setHighlighted(true);
 *   youtube.setSelected(true);
 * </pre>
 *
 * This package also provides a few static auxiliary methods, such as:
 *
 * <pre>
 * var videoId = goog.ui.media.Youtube.parseUrl(
 *     'https://www.youtube.com/watch?v=ddl5f44spwQ');
 * </pre>
 *
 */

goog.provide('goog.ui.media.YoutubeModel');

goog.require('goog.html.TrustedResourceUrl');
goog.require('goog.string');
goog.require('goog.string.Const');

goog.require('goog.ui.media.MediaModel');



/**
 * The `goog.ui.media.Youtube` media data model. It stores a required
 * `videoId` field, sets the youtube URL, and allows a few optional
 * parameters.
 *
 * @param {string} videoId The youtube video id.
 * @param {string=} opt_caption An optional caption of the youtube video.
 * @param {string=} opt_description An optional description of the youtube
 *     video.
 * @constructor
 * @extends {goog.ui.media.MediaModel}
 * @final
 */
goog.ui.media.YoutubeModel = function(videoId, opt_caption, opt_description) {
  'use strict';
  goog.ui.media.MediaModel.call(
      this, goog.ui.media.YoutubeModel.buildUrl(videoId), opt_caption,
      opt_description, goog.ui.media.MediaModel.MimeType.FLASH);

  /**
   * The Youtube video id.
   * @type {string}
   * @private
   */
  this.videoId_ = videoId;

  this.setThumbnails([new goog.ui.media.MediaModel.Thumbnail(
      goog.ui.media.YoutubeModel.getThumbnailUrl(videoId))]);

  this.setPlayer(
      new goog.ui.media.MediaModel.Player(
          goog.ui.media.YoutubeModel.getFlashUrl(videoId, true)));
};
goog.inherits(goog.ui.media.YoutubeModel, goog.ui.media.MediaModel);


/**
 * A youtube regular expression matcher. It matches the VIDEOID of URLs like
 * https://www.youtube.com/watch?v=VIDEOID. Based on:
 * googledata/contentonebox/opencob/specs/common/YTPublicExtractorCard.xml
 * @type {RegExp}
 * @private
 * @const
 */
// Be careful about the placement of the dashes in the character classes. Eg,
// use "[\\w=-]" instead of "[\\w-=]" if you mean to include the dash as a
// character and not create a character range like "[a-f]".
goog.ui.media.YoutubeModel.MATCHER_ = new RegExp(
    // Lead in.
    'https?://(?:[a-zA-Z]{1,3}\\.)?' +
        // Watch short URL prefix and /embed/ URLs. This should handle URLs
        // like:
        // https://youtu.be/jqxENMKaeCU?cgiparam=value
        // https://youtube.com/embed/jqxENMKaeCU?cgiparam=value
        // https://youtube-nocookie.com/jqxENMKaeCU?cgiparam=value
        '(?:(?:(?:youtu\\.be|youtube(?:-nocookie)?\\.com/embed)/([\\w-]+)(?:\\?[\\w=&-]+)?)|' +
        // Watch URL prefix.  This should handle new URLs of the form:
        // https://www.youtube.com/watch#!v=jqxENMKaeCU&feature=related
        // https://www.youtube-nocookie.com/watch#!v=jqxENMKaeCU&feature=related
        // where the parameters appear after "#!" instead of "?".
        '(?:youtube(?:-nocookie)?\\.com/watch)' +
        // Get the video id:
        // The video ID is a parameter v=[videoid] either right after the "?"
        // or after some other parameters.
        '(?:\\?(?:[\\w=-]+&(?:amp;)?)*v=([\\w-]+)' +
        '(?:&(?:amp;)?[\\w=-]+)*)?' +
        // Get any extra arguments in the URL's hash part.
        '(?:#[!]?(?:' +
        // Video ID from the v=[videoid] parameter, optionally surrounded by
        // other
        // & separated parameters.
        '(?:(?:[\\w=-]+&(?:amp;)?)*(?:v=([\\w-]+))' +
        '(?:&(?:amp;)?[\\w=-]+)*)' +
        '|' +
        // Continue supporting "?" for the video ID
        // and "#" for other hash parameters.
        '(?:[\\w=&-]+)' +
        '))?)' +
        // Should terminate with a non-word, non-dash (-) character.
        '[^\\w-]?',
    'i');


/**
 * A auxiliary static method that parses a youtube URL, extracting the ID of the
 * video, and builds a YoutubeModel.
 *
 * @param {string} youtubeUrl A youtube URL.
 * @param {string=} opt_caption An optional caption of the youtube video.
 * @param {string=} opt_description An optional description of the youtube
 *     video.
 * @return {!goog.ui.media.YoutubeModel} The data model that represents the
 *     youtube URL.
 * @see goog.ui.media.YoutubeModel.getVideoId()
 * @throws Error in case the parsing fails.
 */
goog.ui.media.YoutubeModel.newInstance = function(
    youtubeUrl, opt_caption, opt_description) {
  'use strict';
  const extract = goog.ui.media.YoutubeModel.MATCHER_.exec(youtubeUrl);
  if (extract) {
    const videoId = extract[1] || extract[2] || extract[3];
    return new goog.ui.media.YoutubeModel(
        videoId, opt_caption, opt_description);
  }

  throw new Error('failed to parse video id from youtube url: ' + youtubeUrl);
};


/**
 * The opposite of `goog.ui.media.Youtube.newInstance`: it takes a videoId
 * and returns a youtube URL.
 *
 * @param {string} videoId The youtube video ID.
 * @return {string} The youtube URL.
 */
goog.ui.media.YoutubeModel.buildUrl = function(videoId) {
  'use strict';
  return 'https://www.youtube.com/watch?v=' + goog.string.urlEncode(videoId);
};


/**
 * A static auxiliary method that builds a static image URL with a preview of
 * the youtube video.
 *
 * NOTE(goto): patterned after Gmail's gadgets/youtube,
 *
 * TODO(goto): how do I specify the width/height of the resulting image on the
 * url ? is there an official API for https://ytimg.com ?
 *
 * @param {string} youtubeId The youtube video ID.
 * @return {string} An URL that contains an image with a preview of the youtube
 *     movie.
 */
goog.ui.media.YoutubeModel.getThumbnailUrl = function(youtubeId) {
  'use strict';
  return 'https://i.ytimg.com/vi/' + youtubeId + '/default.jpg';
};


/**
 * A static auxiliary method that builds URL of the flash movie to be embedded,
 * out of the youtube video id.
 *
 * @param {string} videoId The youtube video ID.
 * @param {boolean=} opt_autoplay Whether the flash movie should start playing
 *     as soon as it is shown, or if it should show a 'play' button.
 * @return {!goog.html.TrustedResourceUrl} The flash URL to be embedded on the
 *     page.
 */
goog.ui.media.YoutubeModel.getFlashUrl = function(videoId, opt_autoplay) {
  'use strict';
  // YouTube video ids are extracted from youtube URLs, which are user
  // generated input. The video id is later used to embed a flash object,
  // which is generated through HTML construction.
  return goog.html.TrustedResourceUrl.format(
      goog.string.Const.from(
          'https://www.youtube.com/v/%{v}&hl=en&fs=1%{autoplay}'),
      {
        'v': videoId,
        'autoplay': opt_autoplay ? goog.string.Const.from('&autoplay=1') : ''
      });
};


/**
 * Gets the Youtube video id.
 * @return {string} The Youtube video id.
 */
goog.ui.media.YoutubeModel.prototype.getVideoId = function() {
  'use strict';
  return this.videoId_;
};
