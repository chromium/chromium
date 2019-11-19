// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This is the low-level class that generates ChromeVox's
 * earcons. It's designed to be self-contained and not depend on the
 * rest of the code.
 */

goog.provide('EarconEngine');

/**
 * EarconEngine generates ChromeVox's earcons using the web audio API.
 * @constructor
 */
EarconEngine = function() {
  // Public control parameters. All of these are meant to be adjustable.

  /** @type {number} The master volume, as an amplification factor. */
  this.masterVolume = 1.0;

  /** @type {number} The base relative pitch adjustment, in half-steps. */
  this.masterPitch = -4;

  /** @type {number} The click volume, as an amplification factor. */
  this.clickVolume = 0.4;

  /**
   * @type {number} The volume of the static sound, as an
   * amplification factor.
   */
  this.staticVolume = 0.2;

  /** @type {number} The base delay for repeated sounds, in seconds. */
  this.baseDelay = 0.045;

  /** @type {number} The master stereo panning, from -1 to 1. */
  this.masterPan = 0;

  /** @type {number} The master reverb level as an amplification factor. */
  this.masterReverb = 0.4;

  /**
   * @type {string} The choice of the reverb impulse response to use.
   * Must be one of the strings from EarconEngine.REVERBS.
   */
  this.reverbSound = 'small_room_2';

  /** @type {number} The base pitch for the 'wrap' sound in half-steps. */
  this.wrapPitch = 0;

  /** @type {number} The base pitch for the 'alert' sound in half-steps. */
  this.alertPitch = 0;

  /** @type {string} The choice of base sound for most controls. */
  this.controlSound = 'control';

  /**
   * @type {number} The delay between sounds in the on/off sweep effect,
   * in seconds.
   */
  this.sweepDelay = 0.045;

  /**
   * @type {number} The delay between echos in the on/off sweep, in seconds.
   */
  this.sweepEchoDelay = 0.15;

  /** @type {number} The number of echos in the on/off sweep. */
  this.sweepEchoCount = 3;

  /** @type {number} The pitch offset of the on/off sweep, in half-steps. */
  this.sweepPitch = -7;

  /**
   * @type {number} The final gain of the progress sound, as an
   * amplification factor.
   */
  this.progressFinalGain = 0.05;

  /** @type {number} The multiplicative decay rate of the progress ticks. */
  this.progressGain_Decay = 0.7;

  // Private variables.

  /** @type {AudioContext} @private The audio context. */
  this.context_ = new AudioContext();

  /** @type {?ConvolverNode} @private The reverb node, lazily initialized. */
  this.reverbConvolver_ = null;

  /**
   * @type {Object<string, AudioBuffer>} A map between the name of an
   *     audio data file and its loaded AudioBuffer.
   * @private
   */
  this.buffers_ = {};

  /**
   * The source audio nodes for queued tick / tocks for progress.
   * Kept around so they can be canceled.
   *
   * @type {Array<Array<AudioNode>>}
   * @private
   */
  this.progressSources_ = [];

  /** @type {number} The current gain for progress sounds. @private */
  this.progressGain_ = 1.0;

  /** @type {?number} The current time for progress sounds. @private */
  this.progressTime_ = this.context_.currentTime;

  /**
   * @type {?number} The window.setInterval ID for progress sounds.
   * @private
   */
  this.progressIntervalID_ = null;

  // Initialization: load the base sound data files asynchronously.
  var allSoundFilesToLoad = EarconEngine.SOUNDS.concat(EarconEngine.REVERBS);
  allSoundFilesToLoad.forEach((function(sound) {
                                var url =
                                    EarconEngine.BASE_URL + sound + '.wav';
                                this.loadSound(sound, url);
                              }).bind(this));
};

/**
 * @type {Array<string>} The list of sound data files to load.
 * @const
 */
EarconEngine.SOUNDS =
    ['control', 'selection', 'selection_reverse', 'skim', 'static'];

/**
 * @type {Array<string>} The list of reverb data files to load.
 * @const
 */
EarconEngine.REVERBS = ['small_room_2'];

/**
 * @type {number} The scale factor for one half-step.
 * @const
 */
EarconEngine.HALF_STEP = Math.pow(2.0, 1.0 / 12.0);

/**
 * @type {string} The base url for earcon sound resources.
 * @const
 */
EarconEngine.BASE_URL = chrome.extension.getURL('cvox2/background/earcons/');

/**
 * Fetches a sound asynchronously and loads its data into an AudioBuffer.
 *
 * @param {string} name The name of the sound to load.
 * @param {string} url The url where the sound should be fetched from.
 */
EarconEngine.prototype.loadSound = function(name, url) {
  var request = new XMLHttpRequest();
  request.open('GET', url, true);
  request.responseType = 'arraybuffer';

  // Decode asynchronously.
  request.onload = (function() {
                     this.context_.decodeAudioData(
                         /** @type {ArrayBuffer} */ (request.response),
                         (function(buffer) {
                           this.buffers_[name] = buffer;
                         }).bind(this));
                   }).bind(this);
  request.send();
};

/**
 * Return an AudioNode containing the final processing that all
 * sounds go through: master volume / gain, panning, and reverb.
 * The chain is hooked up to the destination automatically, so you
 * just need to connect your source to the return value from this
 * method.
 *
 * @param {{gain: (number | undefined),
 *          pan: (number | undefined),
 *          reverb: (number | undefined)}} properties
 *     An object where you can override the default
 *     gain, pan, and reverb, otherwise these are taken from
 *     masterVolume, masterPan, and masterReverb.
 * @return {AudioNode} The filters to be applied to all sounds, connected
 *     to the destination node.
 */
EarconEngine.prototype.createCommonFilters = function(properties) {
  var gain = this.masterVolume;
  if (properties.gain) {
    gain *= properties.gain;
  }
  var gainNode = this.context_.createGain();
  gainNode.gain.value = gain;
  var first = gainNode;
  var last = gainNode;

  var pan = this.masterPan;
  if (properties.pan !== undefined) {
    pan = properties.pan;
  }
  if (pan != 0) {
    var panNode = this.context_.createPanner();
    panNode.setPosition(pan, 0, -1);
    panNode.setOrientation(0, 0, 1);
    last.connect(panNode);
    last = panNode;
  }

  var reverb = this.masterReverb;
  if (properties.reverb !== undefined) {
    reverb = properties.reverb;
  }
  if (reverb) {
    if (!this.reverbConvolver_) {
      this.reverbConvolver_ = this.context_.createConvolver();
      this.reverbConvolver_.buffer = this.buffers_[this.reverbSound];
      this.reverbConvolver_.connect(this.context_.destination);
    }

    // Dry
    last.connect(this.context_.destination);

    // Wet
    var reverbGainNode = this.context_.createGain();
    reverbGainNode.gain.value = reverb;
    last.connect(reverbGainNode);
    reverbGainNode.connect(this.reverbConvolver_);
  } else {
    last.connect(this.context_.destination);
  }

  return first;
};

/**
 * High-level interface to play a sound from a buffer source by name,
 * with some simple adjustments like pitch change (in half-steps),
 * a start time (relative to the current time, in seconds),
 * gain, panning, and reverb.
 *
 * The only required parameter is the name of the sound. The time, pitch,
 * gain, panning, and reverb are all optional and are passed in an
 * object of optional properties.
 *
 * @param {string} sound The name of the sound to play. It must already
 *     be loaded in a buffer.
 * @param {{pitch: (number | undefined),
 *          time: (number | undefined),
 *          gain: (number | undefined),
 *          pan: (number | undefined),
 *          reverb: (number | undefined)}=} opt_properties
 *     An object where you can override the default pitch, gain, pan,
 *     and reverb.
 * @return {AudioBufferSourceNode} The source node, so you can stop it
 *     or set event handlers on it.
 */
EarconEngine.prototype.play = function(sound, opt_properties) {
  var source = this.context_.createBufferSource();
  source.buffer = this.buffers_[sound];

  if (!opt_properties) {
    // This typecast looks silly, but the Closure compiler doesn't support
    // optional fields in record types very well so this is the shortest hack.
    opt_properties = /** @type {undefined} */ ({});
  }

  var pitch = this.masterPitch;
  if (opt_properties.pitch) {
    pitch += opt_properties.pitch;
  }
  if (pitch != 0) {
    source.playbackRate.value = Math.pow(EarconEngine.HALF_STEP, pitch);
  }

  var destination = this.createCommonFilters(opt_properties);
  source.connect(destination);

  if (opt_properties.time) {
    source.start(this.context_.currentTime + opt_properties.time);
  } else {
    source.start(this.context_.currentTime);
  }

  return source;
};

/**
 * Play the static sound.
 */
EarconEngine.prototype.onStatic = function() {
  this.play('static', {gain: this.staticVolume});
};

/**
 * Play the link sound.
 */
EarconEngine.prototype.onLink = function() {
  this.play('static', {gain: this.clickVolume});
  this.play(this.controlSound, {pitch: 12});
};

/**
 * Play the button sound.
 */
EarconEngine.prototype.onButton = function() {
  this.play('static', {gain: this.clickVolume});
  this.play(this.controlSound);
};

/**
 * Play the text field sound.
 */
EarconEngine.prototype.onTextField = function() {
  this.play('static', {gain: this.clickVolume});
  this.play(
      'static', {time: this.baseDelay * 1.5, gain: this.clickVolume * 0.5});
  this.play(this.controlSound, {pitch: 4});
  this.play(
      this.controlSound, {pitch: 4, time: this.baseDelay * 1.5, gain: 0.5});
};

/**
 * Play the pop up button sound.
 */
EarconEngine.prototype.onPopUpButton = function() {
  this.play('static', {gain: this.clickVolume});

  this.play(this.controlSound);
  this.play(
      this.controlSound, {time: this.baseDelay * 3, gain: 0.2, pitch: 12});
  this.play(
      this.controlSound, {time: this.baseDelay * 4.5, gain: 0.2, pitch: 12});
};

/**
 * Play the check on sound.
 */
EarconEngine.prototype.onCheckOn = function() {
  this.play('static', {gain: this.clickVolume});
  this.play(this.controlSound, {pitch: -5});
  this.play(this.controlSound, {pitch: 7, time: this.baseDelay * 2});
};

/**
 * Play the check off sound.
 */
EarconEngine.prototype.onCheckOff = function() {
  this.play('static', {gain: this.clickVolume});
  this.play(this.controlSound, {pitch: 7});
  this.play(this.controlSound, {pitch: -5, time: this.baseDelay * 2});
};

/**
 * Play the select control sound.
 */
EarconEngine.prototype.onSelect = function() {
  this.play('static', {gain: this.clickVolume});
  this.play(this.controlSound);
  this.play(this.controlSound, {time: this.baseDelay});
  this.play(this.controlSound, {time: this.baseDelay * 2});
};

/**
 * Play the slider sound.
 */
EarconEngine.prototype.onSlider = function() {
  this.play('static', {gain: this.clickVolume});
  this.play(this.controlSound);
  this.play(this.controlSound, {time: this.baseDelay, gain: 0.5, pitch: 2});
  this.play(
      this.controlSound, {time: this.baseDelay * 2, gain: 0.25, pitch: 4});
  this.play(
      this.controlSound, {time: this.baseDelay * 3, gain: 0.125, pitch: 6});
  this.play(
      this.controlSound, {time: this.baseDelay * 4, gain: 0.0625, pitch: 8});
};

/**
 * Play the skim sound.
 */
EarconEngine.prototype.onSkim = function() {
  this.play('skim');
};

/**
 * Play the selection sound.
 */
EarconEngine.prototype.onSelection = function() {
  this.play('selection');
};

/**
 * Play the selection reverse sound.
 */
EarconEngine.prototype.onSelectionReverse = function() {
  this.play('selection_reverse');
};

/**
 * Generate a synthesized musical note based on a sum of sinusoidals shaped
 * by an envelope, controlled by a number of properties.
 *
 * The sound has a frequency of |freq|, or if |endFreq| is specified, does
 * an exponential ramp from |freq| to |endFreq|.
 *
 * If |overtones| is greater than 1, the sound will be mixed with additional
 * sinusoidals at multiples of |freq|, each one scaled by |overtoneFactor|.
 * This creates a rounder tone than a pure sine wave.
 *
 * The envelope is shaped by the duration |dur|, the attack time |attack|,
 * and the decay time |decay|, in seconds.
 *
 * As with other functions, |pan| and |reverb| can be used to override
 * masterPan and masterReverb.
 *
 * @param {{gain: number,
 *          freq: number,
 *          endFreq: (number | undefined),
 *          time: (number | undefined),
 *          overtones: (number | undefined),
 *          overtoneFactor: (number | undefined),
 *          dur: (number | undefined),
 *          attack: (number | undefined),
 *          decay: (number | undefined),
 *          pan: (number | undefined),
 *          reverb: (number | undefined)}} properties
 *     An object containing the properties that can be used to
 *     control the sound, as described above.
 */
EarconEngine.prototype.generateSinusoidal = function(properties) {
  var envelopeNode = this.context_.createGain();
  envelopeNode.connect(this.context_.destination);

  var time = properties.time;
  if (time === undefined) {
    time = 0;
  }

  // Generate an oscillator for the frequency corresponding to the specified
  // frequency, and then additional overtones at multiples of that frequency
  // scaled by the overtoneFactor. Cue the oscillator to start and stop
  // based on the start time and specified duration.
  //
  // If an end frequency is specified, do an exponential ramp to that end
  // frequency.
  var gain = properties.gain;
  for (var i = 0; i < properties.overtones; i++) {
    var osc = this.context_.createOscillator();
    osc.frequency.value = properties.freq * (i + 1);

    if (properties.endFreq) {
      osc.frequency.setValueAtTime(
          properties.freq * (i + 1), this.context_.currentTime + time);
      osc.frequency.exponentialRampToValueAtTime(
          properties.endFreq * (i + 1),
          this.context_.currentTime + properties.dur);
    }

    osc.start(this.context_.currentTime + time);
    osc.stop(this.context_.currentTime + time + properties.dur);

    var gainNode = this.context_.createGain();
    gainNode.gain.value = gain;
    osc.connect(gainNode);
    gainNode.connect(envelopeNode);

    gain *= properties.overtoneFactor;
  }

  // Shape the overall sound by an envelope based on the attack and
  // decay times.
  envelopeNode.gain.setValueAtTime(0, this.context_.currentTime + time);
  envelopeNode.gain.linearRampToValueAtTime(
      1, this.context_.currentTime + time + properties.attack);
  envelopeNode.gain.setValueAtTime(
      1, this.context_.currentTime + time + properties.dur - properties.decay);
  envelopeNode.gain.linearRampToValueAtTime(
      0, this.context_.currentTime + time + properties.dur);

  // Route everything through the common filters like reverb at the end.
  var destination = this.createCommonFilters({});
  envelopeNode.connect(destination);
};

/**
 * Play a sweep over a bunch of notes in a scale, with an echo,
 * for the ChromeVox on or off sounds.
 *
 * @param {boolean} reverse Whether to play in the reverse direction.
 */
EarconEngine.prototype.onChromeVoxSweep = function(reverse) {
  var pitches = [-7, -5, 0, 5, 7, 12, 17, 19, 24];

  if (reverse) {
    pitches.reverse();
  }

  var attack = 0.015;
  var dur = pitches.length * this.sweepDelay;

  var destination = this.createCommonFilters({reverb: 2.0});
  for (var k = 0; k < this.sweepEchoCount; k++) {
    var envelopeNode = this.context_.createGain();
    var startTime = this.context_.currentTime + this.sweepEchoDelay * k;
    var sweepGain = Math.pow(0.3, k);
    var overtones = 2;
    var overtoneGain = sweepGain;
    for (var i = 0; i < overtones; i++) {
      var osc = this.context_.createOscillator();
      osc.start(startTime);
      osc.stop(startTime + dur);

      var gainNode = this.context_.createGain();
      osc.connect(gainNode);
      gainNode.connect(envelopeNode);

      for (var j = 0; j < pitches.length; j++) {
        var freqDecay;
        if (reverse) {
          freqDecay = Math.pow(0.75, pitches.length - j);
        } else {
          freqDecay = Math.pow(0.75, j);
        }
        var gain = overtoneGain * freqDecay;
        var freq = (i + 1) * 220 *
            Math.pow(EarconEngine.HALF_STEP, pitches[j] + this.sweepPitch);
        if (j == 0) {
          osc.frequency.setValueAtTime(freq, startTime);
          gainNode.gain.setValueAtTime(gain, startTime);
        } else {
          osc.frequency.exponentialRampToValueAtTime(
              freq, startTime + j * this.sweepDelay);
          gainNode.gain.linearRampToValueAtTime(
              gain, startTime + j * this.sweepDelay);
        }
        osc.frequency.setValueAtTime(
            freq, startTime + j * this.sweepDelay + this.sweepDelay - attack);
      }

      overtoneGain *= 0.1 + 0.2 * k;
    }

    envelopeNode.gain.setValueAtTime(0, startTime);
    envelopeNode.gain.linearRampToValueAtTime(1, startTime + this.sweepDelay);
    envelopeNode.gain.setValueAtTime(1, startTime + dur - attack * 2);
    envelopeNode.gain.linearRampToValueAtTime(0, startTime + dur);
    envelopeNode.connect(destination);
  }
};

/**
 * Play the "ChromeVox On" sound.
 */
EarconEngine.prototype.onChromeVoxOn = function() {
  this.onChromeVoxSweep(false);
};

/**
 * Play the "ChromeVox Off" sound.
 */
EarconEngine.prototype.onChromeVoxOff = function() {
  this.onChromeVoxSweep(true);
};

/**
 * Play an alert sound.
 */
EarconEngine.prototype.onAlert = function() {
  var freq1 = 220 * Math.pow(EarconEngine.HALF_STEP, this.alertPitch - 2);
  var freq2 = 220 * Math.pow(EarconEngine.HALF_STEP, this.alertPitch - 3);
  this.generateSinusoidal({
    attack: 0.02,
    decay: 0.07,
    dur: 0.15,
    gain: 0.3,
    freq: freq1,
    overtones: 3,
    overtoneFactor: 0.1
  });
  this.generateSinusoidal({
    attack: 0.02,
    decay: 0.07,
    dur: 0.15,
    gain: 0.3,
    freq: freq2,
    overtones: 3,
    overtoneFactor: 0.1
  });
};

/**
 * Play a wrap sound.
 */
EarconEngine.prototype.onWrap = function() {
  this.play('static', {gain: this.clickVolume * 0.3});
  var freq1 = 220 * Math.pow(EarconEngine.HALF_STEP, this.wrapPitch - 8);
  var freq2 = 220 * Math.pow(EarconEngine.HALF_STEP, this.wrapPitch + 8);
  this.generateSinusoidal({
    attack: 0.01,
    decay: 0.1,
    dur: 0.15,
    gain: 0.3,
    freq: freq1,
    endFreq: freq2,
    overtones: 1,
    overtoneFactor: 0.1
  });
};

/**
 * Queue up a few tick/tock sounds for a progress bar. This is called
 * repeatedly by setInterval to keep the sounds going continuously.
 * @private
 */
EarconEngine.prototype.generateProgressTickTocks_ = function() {
  while (this.progressTime_ < this.context_.currentTime + 3.0) {
    var t = this.progressTime_ - this.context_.currentTime;
    this.progressSources_.push([
      this.progressTime_,
      this.play('static', {gain: 0.5 * this.progressGain_, time: t})
    ]);
    this.progressSources_.push([
      this.progressTime_,
      this.play(
          this.controlSound, {pitch: 20, time: t, gain: this.progressGain_})
    ]);

    if (this.progressGain_ > this.progressFinalGain) {
      this.progressGain_ *= this.progressGain_Decay;
    }
    t += 0.5;

    this.progressSources_.push([
      this.progressTime_,
      this.play('static', {gain: 0.5 * this.progressGain_, time: t})
    ]);
    this.progressSources_.push([
      this.progressTime_,
      this.play(
          this.controlSound, {pitch: 8, time: t, gain: this.progressGain_})
    ]);

    if (this.progressGain_ > this.progressFinalGain) {
      this.progressGain_ *= this.progressGain_Decay;
    }

    this.progressTime_ += 1.0;
  }

  var removeCount = 0;
  while (removeCount < this.progressSources_.length &&
         this.progressSources_[removeCount][0] <
             this.context_.currentTime - 0.2) {
    removeCount++;
  }
  this.progressSources_.splice(0, removeCount);
};

/**
 * Start playing tick / tock progress sounds continuously until
 * explicitly canceled.
 */
EarconEngine.prototype.startProgress = function() {
  if (this.progressIntervalID_) {
    this.cancelProgress();
  }

  this.progressSources_ = [];
  this.progressGain_ = 0.5;
  this.progressTime_ = this.context_.currentTime;
  this.generateProgressTickTocks_();
  this.progressIntervalID_ =
      window.setInterval(this.generateProgressTickTocks_.bind(this), 1000);
};

/**
 * Stop playing any tick / tock progress sounds.
 */
EarconEngine.prototype.cancelProgress = function() {
  if (!this.progressIntervalID_) {
    return;
  }

  for (var i = 0; i < this.progressSources_.length; i++) {
    this.progressSources_[i][1].stop();
  }
  this.progressSources_ = [];

  window.clearInterval(this.progressIntervalID_);
  this.progressIntervalID_ = null;
};
