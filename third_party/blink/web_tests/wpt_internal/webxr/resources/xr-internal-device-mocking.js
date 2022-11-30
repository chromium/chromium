'use strict';

/* This file contains extensions to the base mocking from the WebPlatform tests
 * for interal tests. The main mocked objects are found in
 * ../external/wpt/resources/chromium/webxr-test.js. */

// XREnvironmentIntegrationProvider implementation
MockRuntime.prototype.getSubmitFrameCount = function() {
  return this.presentation_provider_.submit_frame_count_;
};

MockRuntime.prototype.getMissingFrameCount = function() {
  return this.presentation_provider_.missing_frame_count_;
};



// Note that if getEnvironmentProvider hasn't finished running yet this will be
// undefined. It's recommended that you allow a successful task to post first
// before attempting to close.
MockRuntime.prototype.closeEnvironmentIntegrationProvider = function() {
  if (this.environmentProviderReceiver_) {
    this.environmentProviderReceiver_.$.close();
  }
};

MockRuntime.prototype.closeDataProvider = function() {
  this.closeEnvironmentIntegrationProvider();
  this.dataProviderReceiver_.$.close();
  this.sessionOptions_ = null;
};

MockRuntime.prototype._injectAdditionalFrameData_preLightEstimation = MockRuntime.prototype._injectAdditionalFrameData;
MockRuntime.prototype._injectAdditionalFrameData = function(options, frameData) {
  this._injectAdditionalFrameData_preLightEstimation(options, frameData);

  if (!options || !options.includeLightingEstimationData) {
    return;
  }

  frameData.lightEstimationData = {
    lightProbe: {
      sphericalHarmonics: {
        coefficients: new Array(9).fill().map((x, i) => ({ red: i, green: i, blue: i })),
      },
      mainLightDirection: { x: 0, y: 1, z: 0 },
      mainLightIntensity: { red: 1, green: 1, blue: 1 },
    },
    reflectionProbe: {
      cubeMap: {
        widthAndHeight: 16,
        positiveX: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        negativeX: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        positiveY: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        negativeY: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        positiveZ: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
        negativeZ: new Array(16 * 16).fill({ red: 0, green: 0, blue: 0, alpha: 0 }),
      },
    },
  };
};

MockRuntime.prototype.setMockAnchorDataIsNull = function(value) {
  if (value && this.anchor_controllers_.size != 0) {
    throw new InvalidStateError("Attempted to mock anchorsData to return null despite already having created anchors!");
  }

  this.mock_anchor_data_is_null_ = value;
};

MockRuntime.prototype._calculateAnchorInformation_preInternal = MockRuntime.prototype._calculateAnchorInformation;
MockRuntime.prototype._calculateAnchorInformation = function(frameData) {
  // Check if anchorsData should was mocked to be returning null.
  // This should be only used in tests that do not actually attempt to create
  // any anchors.
  if (this.mock_anchor_data_is_null_) {
    if (this.anchor_controllers_.size != 0) {
      throw new InvalidStateError("Attempted to mock anchorsData to return null despite already having created anchors!");
    }

    frameData.anchorsData = null;
    return;
  }

  return this._calculateAnchorInformation_preInternal(frameData);
};

ChromeXRTest.prototype.getService = function() {
  return this.mockVRService_;
};

MockVRService.prototype.setFramesThrottledImpl = function(throttled) {
  return this.frames_throttled_ = throttled;
};

MockVRService.prototype.getFramesThrottled = function() {
  // Explicitly converted falsey states (i.e. undefined) to false.
  if (!this.frames_throttled_) {
    return false;
  }

  return this.frames_throttled_;
};
