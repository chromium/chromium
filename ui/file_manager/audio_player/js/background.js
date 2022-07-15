// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SingletonAppWindowWrapper} from '../../file_manager/background/js/app_window_wrapper.js';
import {BackgroundBaseImpl} from '../../file_manager/background/js/background_base.js';
import {FileType} from '../../file_manager/common/js/file_type.js';
import {util} from '../../file_manager/common/js/util.js';

/**
 * Icon of the audio player.
 * Use maximum size and let ash downsample the icon.
 *
 * @type {!string}
 * @const
 */
const AUDIO_PLAYER_ICON = 'icons/audio-player-192.png';

/**
 * HTML source of the audio player as JS module.
 * @type {!string}
 * @const
 */
const AUDIO_PLAYER_APP_URL = 'audio_player.html';

/**
 * Configuration of the audio player.
 * @type {!Object}
 * @const
 */
const audioPlayerCreateOptions = {
  id: 'audio-player',
  minHeight: 4 + 48 + 96,  // 4px: border-top, 48px: track, 96px: controller
  minWidth: 320,
  height: 4 + 48 + 96,  // collapsed
  width: 320,
  frame: {color: '#fafafa'},
};

class AudioPlayerBackground extends BackgroundBaseImpl {
  constructor() {
    super();
  }

  async ready() {
    return await this.initializationPromise_;
  }

  /**
   * Called when an audio player app is restarted.
   */
  onRestarted_() {
    getAudioPlayer.then(audioPlayer => {
      audioPlayer.reopen(function() {
        // If the audioPlayer is reopened, change its window's icon. Otherwise
        // there is no reopened window so just skip the call of setIcon.
        if (audioPlayer.rawAppWindow) {
          audioPlayer.setIcon(AUDIO_PLAYER_ICON);
        }
      });
    });
  }
}

/**
 * Backgound object. This is necessary for AppWindowWrapper.
 * @type {!AudioPlayerBackground}
 */
window.background = new AudioPlayerBackground();


/**
 * Audio player app window wrapper.
 * @type {!Promise<!SingletonAppWindowWrapper>}
 */
const getAudioPlayer = new Promise(async (resolve) => {
  await window.background.ready();
  const url = AUDIO_PLAYER_APP_URL;
  resolve(new SingletonAppWindowWrapper(url, audioPlayerCreateOptions));
});

/**
 * Opens the audio player window.
 * @param {!Array<string>} urls List of audios to play and index to start
 *     playing.
 * @return {!Promise} Promise to be fulfilled on success, or rejected on error.
 */
export async function open(urls) {
  let position = 0;
  const startUrl = (position < urls.length) ? urls[position] : '';

  if (urls.length === 0) {
    throw new Error('No file to open.');
  }

  try {
    const entries = await new Promise(function(fulfill, reject) {
      // Gets the current list of the children of the parent.
      window.webkitResolveLocalFileSystemURL(urls[0], function(fileEntry) {
        fileEntry.getParent(function(parentEntry) {
          const dirReader = parentEntry.createReader();
          let entries = [];

          // Call the reader.readEntries() until no more results are
          // returned.
          const readEntries = function() {
            dirReader.readEntries(function(results) {
              if (!results.length) {
                fulfill(entries.sort(util.compareName));
              } else {
                entries =
                    entries.concat(Array.prototype.slice.call(results, 0));
                readEntries();
              }
            }, reject);
          };

          // Start reading.
          readEntries();
        }, reject);
      }, reject);
    });

    // Omits non-audio files.
    const audioEntries = entries.filter(entry => FileType.isAudio(entry));

    // Adjusts the position to start playing.
    const maybePosition = util.entriesToURLs(audioEntries).indexOf(startUrl);
    if (maybePosition !== -1) {
      position = maybePosition;
    }

    // Opens the audio player.
    const urlsToOpen = util.entriesToURLs(audioEntries);
    const audioPlayer = await getAudioPlayer;
    await audioPlayer.launch({items: urlsToOpen, position: position}, false);

    audioPlayer.setIcon(AUDIO_PLAYER_ICON);
    audioPlayer.rawAppWindow.focus();
    return AUDIO_PLAYER_APP_URL;
  } catch (error) {
    console.error('Launch failed: ' + (error.stack || error));
    throw error;
  }
}

window.background.setLaunchHandler(open);
