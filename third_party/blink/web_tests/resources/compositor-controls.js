// To make sure animation events get processed, periodic lifecycle phase runs
// will include rasterization. See
// https://chromium.googlesource.com/chromium/src/+/main/docs/testing/writing_web_tests.md
// for more information.
function setAnimationRequiresRaster() {
  if (window.testRunner) {
    testRunner.setAnimationRequiresRaster(true);
  }
}

function updateAllLifecyclePhasesAndCompositeAsyncThen(callback) {
  setTimeout(function() {
    if (!window.testRunner) {
      callback();
    } else {
      testRunner.updateAllLifecyclePhasesAndCompositeThen(callback);
    }
  },0);
}
