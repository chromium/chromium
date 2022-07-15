// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';


/**
 * @typedef {?{
 *   url: string,
 *   title: string,
 *   artist: string,
 *   artworkUrl: string,
 *   active: boolean
 * }}
 */
export let TrackInfo;

Polymer({
  _template: html`{__html_template__}`,

  is: 'track-list',

  properties: {
    /**
     * List of tracks.
     */
    tracks: {
      type: Array,
      value: [],
      observer: 'tracksChanged',
    },

    /**
     * Track index of the current track.
     * If the tracks property is empty, it should be -1. Otherwise, be a valid
     * track number.
     */
    currentTrackIndex: {
      type: Number,
      value: -1,
      observer: 'currentTrackIndexChanged',
      notify: true,
    },

    /**
     * Whether shuffling play order is enabled or not.
     */
    shuffle: {
      type: Boolean,
      value: false,
      observer: 'shuffleChanged',
    },

    /**
     * Whether playlist is expanded or not.
     */
    expanded: {
      type: Boolean,
      value: false,
      observer: 'expandedChanged',
    },
  },

  /**
   * Play order of the tracks. Each value is the index of 'this.tracks'.
   * @type {Array<number>}
   */
  playOrder: [],

  /**
   * Invoked when 'expanded' property is changed.
   * @param {boolean} newValue New value.
   * @param {boolean} oldValue Old value.
   */
  expandedChanged: function(newValue, oldValue) {
    this.ensureTrackInViewport_(this.currentTrackIndex);
  },

  /**
   * Invoked when 'shuffle' property is changed.
   * @param {boolean} newValue New value.
   * @param {boolean} oldValue Old value.
   */
  shuffleChanged: function(newValue, oldValue) {
    this.generatePlayOrder(true /* keep the current track */);
  },

  /**
   * Invoked when the current track index is changed.
   * @param {number} newValue new value.
   * @param {number} oldValue old value.
   */
  currentTrackIndexChanged: function(newValue, oldValue) {
    if (oldValue === newValue) {
      return;
    }

    if (!isNaN(oldValue) && 0 <= oldValue && oldValue < this.tracks.length) {
      this.set('tracks.' + oldValue + '.active', false);
    }

    if (0 <= newValue && newValue < this.tracks.length) {
      const currentPlayOrder = this.playOrder.indexOf(newValue);
      if (currentPlayOrder !== -1) {
        // Success
        this.set('tracks.' + newValue + '.active', true);

        this.ensureTrackInViewport_(newValue /* trackIndex */);
        return;
      }
    }

    // Invalid index
    if (this.tracks.length === 0) {
      this.currentTrackIndex = -1;
    } else {
      this.generatePlayOrder(false /* no need to keep the current track */);
    }
  },

  /**
   * Invoked when 'tracks' property is changed.
   * @param {Array<!TrackInfo>} newValue New value.
   * @param {Array<!TrackInfo>} oldValue Old value.
   */
  tracksChanged: function(newValue, oldValue) {
    // Note: Sometimes both oldValue and newValue are null though the actual
    // values are not null. Maybe it's a bug of Polymer.

    if (this.tracks.length !== 0) {
      // Restore the active track.
      if (this.currentTrackIndex !== -1 &&
          this.currentTrackIndex < this.tracks.length) {
        this.set('tracks.' + this.currentTrackIndex + '.active', true);
      }

      // Reset play order and current index.
      this.generatePlayOrder(false /* no need to keep the current track */);
    } else {
      this.playOrder = [];
      this.currentTrackIndex = -1;
    }
  },

  /**
   * Invoked when the track element is clicked.
   * @param {Event} event Click event.
   */
  trackClicked: function(event) {
    const index = ~~event.currentTarget.getAttribute('index');
    const track = this.tracks[index];
    if (track) {
      this.selectTrack(track);
    }
  },

  /**
   * Scrolls the track list to ensure the given track in the viewport.
   * @param {number} trackIndex The index of the track to be in the viewport.
   * @private
   */
  ensureTrackInViewport_: function(trackIndex) {
    const trackElement = this.$$('.track[index="' + trackIndex + '"]');
    if (trackElement) {
      const viewTop = this.scrollTop;
      const viewHeight = this.clientHeight;
      const elementTop = trackElement.offsetTop - this.offsetTop;
      const elementHeight = trackElement.offsetHeight;

      if (elementTop <= viewTop) {
        // Adjust the tops.
        this.scrollTop = elementTop;
      } else if (elementTop + elementHeight >= viewTop + viewHeight) {
        // Adjust the bottoms.
        this.scrollTop = Math.max(0, (elementTop + elementHeight - viewHeight));
      } else {
        // The entire element is in the viewport. Do nothing.
      }
    }
  },

  /**
   * Invoked when the track element is clicked.
   * @param {boolean} keepCurrentTrack Keep the current track or not.
   */
  generatePlayOrder: function(keepCurrentTrack) {
    console.assert(
        (keepCurrentTrack !== undefined),
        'The argument "forward" is undefined');

    if (this.tracks.length === 0) {
      this.playOrder = [];
      return;
    }

    // Creates sequenced array.
    this.playOrder = this.tracks.map(function(unused, index) {
      return index;
    });

    if (this.shuffle) {
      // Randomizes the play order array (Schwarzian-transform algorithm).
      this.playOrder = this.playOrder
                           .map(function(a) {
                             return {weight: Math.random(), index: a};
                           })
                           .sort(function(a, b) {
                             return a.weight - b.weight;
                           })
                           .map(function(a) {
                             return a.index;
                           });

      if (keepCurrentTrack) {
        // Puts the current track at the beginning of the play order.
        this.playOrder = this.playOrder.filter(function(value) {
          return this.currentTrackIndex !== value;
        }, this);
        this.playOrder.splice(0, 0, this.currentTrackIndex);
      }
    }

    if (!keepCurrentTrack) {
      this.currentTrackIndex = this.playOrder[0];
    }
  },

  /**
   * Sets the current track.
   * @param {!TrackInfo} track TrackInfo to be set as the current
   *     track.
   */
  selectTrack: function(track) {
    let index = -1;
    for (let i = 0; i < this.tracks.length; i++) {
      if (this.tracks[i].url === track.url) {
        index = i;
        break;
      }
    }
    if (index >= 0) {
      if (this.currentTrackIndex === index) {
        this.fire('replay');
      } else {
        this.currentTrackIndex = index;
        this.fire('play');
      }
    }
  },

  /**
   * Returns the current track.
   * @return {TrackInfo} track TrackInfo of the current track.
   */
  getCurrentTrack: function() {
    if (this.tracks.length === 0) {
      return null;
    }

    return this.tracks[this.currentTrackIndex];
  },

  /**
   * Returns the next (or previous) track in the track list. If there is no
   * next track, returns -1.
   *
   * @param {boolean} forward Specify direction: forward or previous mode.
   *     True: forward mode, false: previous mode.
   * @param {boolean} cyclic Specify if cyclically or not: It true, the first
   *     track is succeeding to the last track, otherwise no track after the
   *     last.
   * @return {number} The next track index.
   */
  getNextTrackIndex: function(forward, cyclic) {
    if (this.tracks.length === 0) {
      return -1;
    }

    const defaultTrackIndex =
        forward ? this.playOrder[0] : this.playOrder[this.tracks.length - 1];

    const currentPlayOrder = this.playOrder.indexOf(this.currentTrackIndex);
    console.assert(
        (0 <= currentPlayOrder && currentPlayOrder < this.tracks.length),
        'Insufficient TrackList.playOrder. The current track is not on the ' +
            'track list.');

    const newPlayOrder = currentPlayOrder + (forward ? +1 : -1);
    if (newPlayOrder === -1 || newPlayOrder === this.tracks.length) {
      return cyclic ? defaultTrackIndex : -1;
    }

    const newTrackIndex = this.playOrder[newPlayOrder];
    console.assert(
        (0 <= newTrackIndex && newTrackIndex < this.tracks.length),
        'Insufficient TrackList.playOrder. New Play Order: ' + newPlayOrder);

    return newTrackIndex;
  },
});
