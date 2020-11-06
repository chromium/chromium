// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!number}
 * @private
 */
const QR_CODE_DETECTION_INTERVAL_MS = 1000;

/** @enum {number} */
const PageState = {
  INITIAL: 1,
  SCANNING_USER_FACING: 2,
  SCANNING_ENVIRONMENT_FACING: 3,
  SWITCHING_CAM_USER_TO_ENVIRONMENT: 4,
  SWITCHING_CAM_ENVIRONMENT_TO_USER: 5,
  SUCCESS: 6,
};

/** @enum {number} */
const UiElement = {
  START_SCANNING: 1,
  VIDEO: 2,
  SCAN_SUCCESS: 3,
  SWITCH_CAMERA: 4,
};

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

    /**
     * @type {!PageState}
     * @private
     */
    state_: {
      type: Object,
      value: PageState,
    },

    /** @private */
    hasMultipleCameras_: {
      type: Boolean,
      value: false,
      observer: 'onHasMultipleCamerasChanged_',
    },

    /**
     * Enum used as an ID for specific UI elements.
     * A UiElement is passed between html and JS for
     * certain UI elements to determine their state.
     *
     * @type {!UiElement}
     */
    UiElement: {
      type: Object,
      value: UiElement,
    },
  },

  /**
   * @type {MediaDevices}
   * @private
   */
  mediaDevices_: null,

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

  /** @override */
  ready() {
    this.setMediaDevices(navigator.mediaDevices);
    this.state_ = PageState.INITIAL;
  },

  /** @override */
  detached() {
    if (this.stream_) {
      this.stream_.getTracks()[0].stop();
    }
    if (this.qrCodeDetectorTimer_) {
      clearTimeout(this.qrCodeDetectorTimer_);
    }
    this.mediaDevices_.removeEventListener(
        'devicechange', this.updateHasMultipleCameras_.bind(this));
  },

  /**
   * @param {MediaDevices} mediaDevices
   */
  setMediaDevices(mediaDevices) {
    this.mediaDevices_ = mediaDevices;
    this.mediaDevices_.addEventListener(
        'devicechange', this.updateHasMultipleCameras_.bind(this));
  },

  /** @private */
  updateHasMultipleCameras_() {
    this.mediaDevices_.enumerateDevices().then(devices => {
      const numVideoInputDevices =
          devices.filter(device => device.kind === 'videoinput').length;
      this.hasMultipleCameras_ = numVideoInputDevices > 1;
    });
  },

  /** @private */
  onHasMultipleCamerasChanged_() {
    // If the user was using an environment-facing camera and it was removed,
    // restart scanning with the user-facing camera.
    if ((this.state_ === PageState.SCANNING_ENVIRONMENT_FACING) &&
        !this.hasMultipleCameras_) {
      this.state_ = PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
      this.startScanning_();
    }
  },

  /** private */
  startScanning_() {
    const oldStream = this.stream_;
    if (this.qrCodeDetectorTimer_) {
      clearTimeout(this.qrCodeDetectorTimer_);
    }

    const useUserFacingCamera =
        this.state_ !== PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT;
    this.mediaDevices_
        .getUserMedia({
          video: {
            height: 130,
            width: 482,
            facingMode: useUserFacingCamera ? 'user' : 'environment'
          },
          audio: false
        })
        .then(stream => {
          this.stream_ = stream;
          if (stream) {
            const video = this.$.video;
            video.srcObject = stream;
            video.play();
          }
          if (oldStream) {
            oldStream.getTracks()[0].stop();
          }

          this.activationCode_ = '';
          this.state_ = useUserFacingCamera ?
              PageState.SCANNING_USER_FACING :
              PageState.SCANNING_ENVIRONMENT_FACING;

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
      this.state_ = PageState.SUCCESS;
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

  /** @private */
  onSwitchCameraButtonPressed_() {
    if (this.state_ === PageState.SCANNING_USER_FACING) {
      this.state_ = PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT;
    } else if (this.state_ === PageState.SCANNING_ENVIRONMENT_FACING) {
      this.state_ = PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
    }
    this.startScanning_();
  },

  /**
   * @param {UiElement} uiElement
   * @param {PageState} state
   * @param {boolean} hasMultipleCameras
   * @private
   */
  isUiElementHidden_(uiElement, state, hasMultipleCameras) {
    switch (uiElement) {
      case UiElement.START_SCANNING:
        return state !== PageState.INITIAL;
      case UiElement.VIDEO:
        return state !== PageState.SCANNING_USER_FACING &&
            state !== PageState.SCANNING_ENVIRONMENT_FACING;
      case UiElement.SCAN_SUCCESS:
        return state !== PageState.SUCCESS;
      case UiElement.SWITCH_CAMERA:
        const isScanning = state === PageState.SCANNING_USER_FACING ||
            state === PageState.SCANNING_ENVIRONMENT_FACING;
        return !(isScanning && hasMultipleCameras);
    }
  },

  /**
   * @param {UiElement} uiElement
   * @param {PageState} state
   * @private
   */
  isUiElementDisabled_(uiElement, state) {
    switch (uiElement) {
      case UiElement.SWITCH_CAMERA:
        return state === PageState.SWITCHING_CAM_USER_TO_ENVIRONMENT ||
            state === PageState.SWITCHING_CAM_ENVIRONMENT_TO_USER;
      default:
        return false;
    }
  },
});
