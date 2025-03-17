// META: global=window,dedicatedworker
// META: script=/webcodecs/utils.js

// TODO(crbug.com/1274220): Move these back to the public wpt test set once
// HDR canvas support has actually launched. Currently there's no way to feature
// detect HDR canvas support from a worker.

promise_test(() => {
  return testImageBitmapToAndFromVideoFrame(
      48, 36, kP3Pixel, {colorSpace: 'display-p3'}, {colorSpaceConversion: 'none'},
      {colorSpace: 'display-p3'});
}, 'ImageBitmap<->VideoFrame with canvas(48x36 display-p3 uint8).');

promise_test(() => {
  return testImageBitmapToAndFromVideoFrame(
      480, 360, kP3Pixel, {colorSpace: 'display-p3'}, {colorSpaceConversion: 'none'},
      {colorSpace: 'display-p3'});
}, 'ImageBitmap<->VideoFrame with canvas(480x360 display-p3 uint8).');

