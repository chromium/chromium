// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/foreground/elements/files_icon_button.js';
import './repeat_button.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/**
 * @typedef {?{
 *   mute: string,
 *   next: string,
 *   pause: string,
 *   play: string,
 *   playList: string,
 *   previous: string,
 *   repeat: string,
 *   seekSlider: string,
 *   shuffle: string,
 *   unmute: string,
 *   volumeSlider: string,
 * }}
 */
export let AriaLabels;

Polymer({
  _template: html`{__html_template__}`,

  is: 'control-panel',

  properties: {
    /**
     * Flag whether the audio is playing or paused. True if playing, or false
     * paused.
     */
    playing: {
      type: Boolean,
      value: false,
      notify: true,
      reflectToAttribute: true,
      observer: 'playingChanged_',
    },

    /**
     * Current elapsed time in the current music in millisecond.
     */
    time: {
      type: Number,
      value: 0,
    },

    /**
     * Total length of the current music in millisecond.
     */
    duration: {
      type: Number,
      value: 0,
    },

    /**
     * Whether the shuffle button is ON.
     */
    shuffle: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * What mode the repeat button indicates.
     * repeat-modes can be "no-repeat", "repeat-all", "repeat-one".
     */
    repeatMode: {
      type: String,
      value: 'no-repeat',
      notify: true,
    },

    /**
     * The audio volume. 0 is silent, and 100 is maximum loud.
     */
    volume: {
      type: Number,
      value: 50,
      notify: true,
      reflectToAttribute: true,
      observer: 'volumeChanged_',
    },

    /**
     * Whether the playlist is expanded or not.
     */
    playlistExpanded: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Whether the knob of time slider is being dragged.
     */
    dragging: {
      type: Boolean,
      value: false,
      notify: true,
    },

    /**
     * Dictionary which contains aria-labels for each controls.
     * @type {AriaLabels}
     */
    ariaLabels: {
      type: Object,
      observer: 'ariaLabelsChanged_',
    },
  },

  /**
   * Initializes an element. This method is called automatically when the
   * element is ready.
   */
  ready: function() {
    const timeSlider = /** @type {!CrSliderElement} */ (this.$.timeSlider);
    timeSlider.addEventListener('cr-slider-value-changed', () => {
      this.fire('update-time', timeSlider.value);
    });

    const volumeSlider =
        /** @type {!CrSliderElement} */ (this.$.volumeSlider);
    volumeSlider.addEventListener('cr-slider-value-changed', () => {
      this.volume = volumeSlider.value;
    });
  },

  /**
   * Invoked when the next button is clicked.
   */
  nextClick: function() {
    this.fire('next-clicked');
  },

  /**
   * Invoked when the play button is clicked.
   */
  playClick: function() {
    this.playing = !this.playing;
  },

  /**
   * Invoked when the previous button is clicked.
   */
  previousClick: function() {
    this.fire('previous-clicked');
  },

  /**
   * Invoked when the volume button is clicked.
   */
  volumeClick: function() {
    if (this.volume !== 0) {
      this.savedVolume_ = this.volume;
      this.volume = 0;
    } else {
      this.volume = this.savedVolume_ || 50;
    }
  },

  /**
   * @param {boolean} forward Whether to skip forward/backword.
   */
  smallSkip: function(forward) {
    this.skip_(true /* small */, forward);
  },

  /**
   * @param {boolean} forward Whether to skip forward/backword.
   */
  bigSkip: function(forward) {
    this.skip_(false /* small */, forward);
  },

  /**
   * Skips small min(5 seconds, 10% of duration) or large
   * min(10 seconds, 20% of duration).
   * @param {boolean} small Whether to skip small/large interval.
   * @param {boolean} forward Whether to skip forward/backword.
   * @private
   */
  skip_: function(small, forward) {
    const maxSkip = small ? 5000 : 10000;
    const percentOfDuration = (small ? .1 : .2) * this.duration;
    const update = (forward ? 1 : -1) * Math.min(maxSkip, percentOfDuration);
    if (this.duration > 0) {
      this.fire(
          'update-time',
          Math.max(Math.min(this.time + update, this.duration), 0));
    }
  },

  /**
   * Converts the time into human friendly string.
   * @param {number} time Time to be converted.
   * @return {string} String representation of the given time
   */
  time2string_: function(time) {
    return ~~(time / 60000) + ':' + ('0' + ~~(time / 1000 % 60)).slice(-2);
  },

  /**
   * Converts the time and duration into human friendly string.
   * @param {number} time Time to be converted.
   * @param {number} duration Duration to be converted.
   * @return {string} String representation of the given time
   */
  computeTimeString_: function(time, duration) {
    return this.time2string_(time) + ' / ' + this.time2string_(duration);
  },

  /**
   * Invoked when the playing property is changed.
   * @param {boolean} playing
   * @private
   */
  playingChanged_: function(playing) {
    if (this.ariaLabels) {
      this.$.play.setAttribute(
          'aria-label', playing ? this.ariaLabels.pause : this.ariaLabels.play);
    }
  },

  /**
   * Invoked when the volume property is changed.
   * @param {number} volume
   * @private
   */
  volumeChanged_: function(volume) {
    if (!this.$.volumeSlider.dragging) {
      this.$.volumeSlider.value = volume;
    }

    if (this.ariaLabels) {
      this.$.volumeButton.setAttribute(
          'aria-label',
          volume !== 0 ? this.ariaLabels.mute : this.ariaLabels.unmute);
    }
  },

  /**
   * @param {!CustomEvent<{value: boolean}>} e
   * @private
   */
  onSeekingChanged_: function(e) {
    this.fire('seeking-changed', e.detail);
  },

  /**
   * Invoked when the ariaLabels property is changed.
   * @param {Object} ariaLabels
   * @private
   */
  ariaLabelsChanged_: function(ariaLabels) {
    assert(ariaLabels);
    // TODO(fukino): Use data bindings.
    this.$.volumeSlider.setAttribute('aria-label', ariaLabels.volumeSlider);
    this.$.shuffle.setAttribute('aria-label', ariaLabels.shuffle);
    this.$.repeat.setAttribute('aria-label', ariaLabels.repeat);
    this.$.previous.setAttribute('aria-label', ariaLabels.previous);
    this.$.play.setAttribute(
        'aria-label', this.playing ? ariaLabels.pause : ariaLabels.play);
    this.$.next.setAttribute('aria-label', ariaLabels.next);
    this.$.playList.setAttribute('aria-label', ariaLabels.playList);
    this.$.timeSlider.setAttribute('aria-label', ariaLabels.seekSlider);
    this.$.volumeButton.setAttribute(
        'aria-label', this.volume !== 0 ? ariaLabels.mute : ariaLabels.unmute);
    this.$.volumeSlider.setAttribute('aria-label', ariaLabels.volumeSlider);
  },
});
