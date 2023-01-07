/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.media.YoutubeTest');
goog.setTestOnly();

const YoutubeModel = goog.require('goog.ui.media.YoutubeModel');
const testSuite = goog.require('goog.testing.testSuite');

const YOUTUBE_VIDEO_ID = 'dMH0bHeiRNg';
const YOUTUBE_URL = `https://www.youtube.com/watch?v=${YOUTUBE_VIDEO_ID}`;

function assertExtractsCorrectly(expectedVideoId, url) {
  const youtube = YoutubeModel.newInstance(url);
  assertEquals(`videoid for ${url}`, expectedVideoId, youtube.getVideoId());
}
testSuite({
  setUp() {
  },

  tearDown() {
  },

  testParsingUrl() {
    // a simple link
    assertExtractsCorrectly(
        'uddeBVmKTqE', 'http://www.youtube.com/watch?v=uddeBVmKTqE');
    // a simple mobile link
    assertExtractsCorrectly(
        'uddeBVmKTqE', 'http://m.youtube.com/watch?v=uddeBVmKTqE');
    // a secure mobile link
    assertExtractsCorrectly(
        'uddeBVmKTqE', 'https://m.youtube.com/watch?v=uddeBVmKTqE');
    // a simple youtube-nocookie link
    assertExtractsCorrectly(
        'uddeBVmKTqE', 'http://www.youtube-nocookie.com/watch?v=uddeBVmKTqE');
    // a simple /embed/ link
    assertExtractsCorrectly(
        'uddeBVmKTqE', 'http://www.youtube.com/embed/uddeBVmKTqE?controls=0');
    // a -nocookie /embed/ link
    assertExtractsCorrectly(
        'uddeBVmKTqE',
        'http://www.youtube-nocookie.com/embed/uddeBVmKTqE?controls=0');
    // a simple short link
    assertExtractsCorrectly('uddeBVmKTqE', 'http://youtu.be/uddeBVmKTqE');
    // a secure short link
    assertExtractsCorrectly('uddeBVmKTqE', 'https://youtu.be/uddeBVmKTqE');
    // a secure short link with a CGI parameter
    assertExtractsCorrectly(
        'uddeBVmKTqE', 'https://youtu.be/uddeBVmKTqE?feature=channel');
    // a channel link
    assertExtractsCorrectly(
        '4Pb9e1uu3EQ',
        'http://www.youtube.com/watch?v=4Pb9e1uu3EQ&feature=channel');
    // a UK link
    assertExtractsCorrectly(
        'xqWXO87TlH4',
        'http://uk.youtube.com/watch?gl=GB&hl=en-GB&v=xqWXO87TlH4');
    // an India link
    assertExtractsCorrectly(
        '10FKWOn4qGA',
        'http://www.youtube.com/watch?gl=IN&hl=en-GB&v=10FKWOn4qGA');
    // an ad
    assertExtractsCorrectly(
        'wk1_kDJhyBk',
        'http://www.youtube.com/watch?v=wk1_kDJhyBk&feature=yva-video-display');
    // a related video
    assertExtractsCorrectly(
        '7qL2PuLF0SI',
        'http://www.youtube.com/watch?v=7qL2PuLF0SI&feature=related');
    // with a timestamp
    assertExtractsCorrectly(
        'siJZXtsdfsf', 'http://www.youtube.com/watch?v=siJZXtsdfsf#t=2m59s');
    // with a timestamp and multiple hash params
    assertExtractsCorrectly(
        'siJZXtabdef',
        'http://www.youtube.com/watch?v=siJZXtabdef#t=1m59s&videos=foo');
    // with a timestamp, multiple regular and hash params
    assertExtractsCorrectly(
        'siJZXtabxyz',
        'http://www.youtube.com/watch?foo=bar&v=siJZXtabxyz&x=y#t=1m30s' +
            '&videos=bar');
    // only hash params
    assertExtractsCorrectly(
        'MWBpQoPwT3U',
        'http://www.youtube.com/watch#!playnext=1&playnext_from=TL' +
            '&videos=RX1XPmgerGo&v=MWBpQoPwT3U');
    // only hash params
    assertExtractsCorrectly(
        'MWBpQoPwT3V',
        'http://www.youtube.com/watch#!playnext=1&playnext_from=TL' +
            '&videos=RX1XPmgerGp&v=MWBpQoPwT3V&foo=bar');
    assertExtractsCorrectly(
        'jqxENMKaeCU',
        'http://www.youtube.com/watch#!v=jqxENMKaeCU&feature=related');
    // Lots of query params, some of them w/ numbers, one of them before the
    // video ID
    assertExtractsCorrectly(
        'qbce2yN81mE',
        'http://www.youtube.com/watch?usg=AFQjCNFf90T3fekgdVBmPp-Wgya5_CTSaw' +
            '&v=qbce2yN81mE&source=video&vgc=rss');
    assertExtractsCorrectly(
        'Lc-8onVA5Jk',
        'http://www.youtube.com/watch?v=Lc-8onVA5Jk&feature=dir');
    // Last character in the video ID is '-' (a non-word but valid character)
    // and the video ID is the last query parameter
    assertExtractsCorrectly(
        'Lc-8onV5Jk-', 'http://www.youtube.com/watch?v=Lc-8onV5Jk-');

    const invalidUrls = [
      'http://invalidUrl/watch?v=dMH0bHeiRNg',
      'http://www$youtube.com/watch?v=dMH0bHeiRNg',
      'http://www.youtube$com/watch?v=dMH0bHeiRNg',
      'http://w_w.youtube.com/watch?v=dMH0bHeiRNg',
    ];
    for (let i = 0, j = invalidUrls.length; i < j; ++i) {
      const e = assertThrows('parser expects a well formed URL', () => {
        YoutubeModel.newInstance(invalidUrls[i]);
      });
      assertEquals(
          'failed to parse video id from youtube url: ' + invalidUrls[i],
          e.message);
    }
  },

  testBuildingUrl() {
    assertEquals(YOUTUBE_URL, YoutubeModel.buildUrl(YOUTUBE_VIDEO_ID));
  },

  testCreatingModel() {
    const model = new YoutubeModel(YOUTUBE_VIDEO_ID);
    assertEquals(YOUTUBE_VIDEO_ID, model.getVideoId());
    assertEquals(YOUTUBE_URL, model.getUrl());
    assertUndefined(model.getCaption());
  },

  testUrlMatcher() {
    /** @suppress {visibility} suppression added to enable type checking */
    const matcher = YoutubeModel.MATCHER_;
    assertTrue(matcher.test('http://www.youtube.com/watch?v=55D-ybnYQSs'));
    assertTrue(matcher.test('https://youtube.com/watch?v=55D-ybnYQSs'));
    assertTrue(
        matcher.test('https://youtube.com/watch?blarg=blop&v=55D-ybnYQSs'));
    assertTrue(matcher.test('http://www.youtube.com/watch?v=55D-ybnYQSs#wee'));

    assertFalse(matcher.test('http://www.cnn.com/watch?v=55D-ybnYQSs#wee'));
    assertFalse(matcher.test('ftp://www.youtube.com/watch?v=55D-ybnYQSs#wee'));
  },
});
