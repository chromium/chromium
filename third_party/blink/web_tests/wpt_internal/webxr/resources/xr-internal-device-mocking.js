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
