'use strict';

/* This file contains extensions to the base mocking from the WebPlatform tests
 * for interal tests. The main mocked objects are found in
 * ../external/wpt/resources/chromium/webxr-test.js. */
MockRuntime.prototype.setHitTestResults = function(results) {
  this.hittest_results_ = results;
};

MockRuntime.prototype.requestHitTest = function(ray) {
  var hit_results = this.hittest_results_;
  if (!hit_results) {
    var hit = new device.mojom.XRHitResult();

    // No change to the underlying matrix/leaving it null results in identity.
    hit.hitMatrix = new gfx.mojom.Transform();
    hit_results = {results: [hit]};
  }
  return Promise.resolve(hit_results);
};

MockRuntime.prototype.setStageSize = function(x, z) {
  if (!this.displayInfo_.stageParameters) {
    this.displayInfo_.stageParameters = default_stage_parameters;
  }

  this.displayInfo_.stageParameters.sizeX = x;
  this.displayInfo_.stageParameters.sizeZ = z;

  this.sessionClient_.onChanged(this.displayInfo_);
};

MockRuntime.prototype.getSubmitFrameCount = function() {
  return this.presentation_provider_.submit_frame_count_;
};

MockRuntime.prototype.getMissingFrameCount = function() {
  return this.presentation_provider_.missing_frame_count_;
};

// Patch in experimental features.
MockRuntime.featureToMojoMap["dom-overlay-for-handheld-ar"] =
    device.mojom.XRSessionFeature.DOM_OVERLAY_FOR_HANDHELD_AR;

ChromeXRTest.prototype.getService = function() {
  return this.mockVRService_;
}

MockVRService.prototype.setFramesThrottled = function(throttled) {
  return this.frames_throttled_ = throttled;
}

MockVRService.prototype.getFramesThrottled = function() {
  // Explicitly converted falsey states (i.e. undefined) to false.
  if (!this.frames_throttled_) {
    return false;
  }

  return this.frames_throttled_;
};

