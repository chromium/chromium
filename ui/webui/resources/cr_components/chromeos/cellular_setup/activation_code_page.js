// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!number}
 * @private
 */
const QR_CODE_DETECTION_INTERVAL_MS = 1000;

/**
 * Page in eSIM Setup flow that accepts activation code. User has option for
 * manual entry or scan a QR code.
 */
Polymer({
  is: 'activation-code-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @private */
    activationCode_: {
      type: String,
      value: '',
      observer: 'onActivationCodeChanged_',
    },

    /** @private */
    qrCodeScanInProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isInitialState_: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * @type {?MediaStream}
   * @private
   */
  stream_: null,

  /**
   * @type {?number}
   * @private
   */
  qrCodeDetectorTimer_: null,

  /** override */
  detached() {
    if (this.stream_) {
      this.stream_.getTracks()[0].stop();
    }
    if (this.qrCodeDetectorTimer_) {
      clearTimeout(this.qrCodeDetectorTimer_);
    }
  },

  /** private */
  startScanning_() {
    // TODO(crbug.com/1093185): Add logic for changing stream if user flips
    // camera. Add error handling for camera not working.
    navigator.mediaDevices
        .getUserMedia({video: {height: 130, width: 482}, audio: false})
        .then(stream => {
          this.stream_ = stream;
          const video = this.$.video;
          video.srcObject = stream;
          video.play();

          this.activationCode_ = '';
          this.qrCodeScanInProgress_ = true;
          this.isInitialState_ = false;

          this.detectQrCode_(stream);
        });
  },

  /**
   * Continuously checks stream if it contains a QR code. If a QR code is
   * detected, activationCode_ is set to the QR code's value and the detection
   * stops.
   * @param {MediaStream} stream
   * @private
   */
  async detectQrCode_(stream) {
    try {
      this.qrCodeDetectorTimer_ = setInterval(
          (async function() {
            const capturer = new ImageCapture(stream.getVideoTracks()[0]);
            const frame = await capturer.grabFrame();
            const activationCode = await this.detectActivationCode_(frame);
            if (activationCode) {
              clearTimeout(this.qrCodeDetectorTimer_);
              this.activationCode_ = activationCode;
              this.qrCodeScanInProgress_ = false;
            }
          }).bind(this),
          QR_CODE_DETECTION_INTERVAL_MS);
    } catch (error) {
      // TODO(crbug.com/1093185): Update the UI in response to the error.
      console.log(error);
    }
  },

  /**
   * @param {ImageBitmap} frame
   * @return {!Promise<string|null>}
   * TODO(crbug.com/1093185): Remove suppression when shape_detection extern
   * definitions become available.
   * @suppress {undefinedVars|missingProperties}
   * @private
   */
  async detectActivationCode_(frame) {
    const qrCodeDetector = new BarcodeDetector({
      formats: [
        'qr_code',
      ]
    });
    const qrCodes = await qrCodeDetector.detect(frame);
    if (qrCodes.length > 0) {
      return qrCodes[0].rawValue;
    }
    return null;
  },

  /** @private */
  onActivationCodeChanged_() {
    const activationCode = this.validateActivationCode_(this.activationCode_);
    this.fire('activation-code-updated', {activationCode: activationCode});
    // TODO(crbug.com/1093185): Handle if activation code is invalid.
    if (activationCode) {
      if (this.stream_) {
        this.stream_.getTracks()[0].stop();
      }
    }
  },

  /**
   * @param {string} activationCode
   * @return {string|null} The validated activation code or null if it's
   *     invalid.
   * @private
   */
  validateActivationCode_(activationCode) {
    // TODO(crbug.com/1093185): Add better validation when we know the
    // constraints.
    activationCode = activationCode.trim();
    if (activationCode.length > 3) {
      return activationCode;
    }
    return null;
  },
});
